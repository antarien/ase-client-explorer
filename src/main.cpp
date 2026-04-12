/**
 * ASE Project Explorer — GTK4 Desktop Application
 *
 * Standalone hierarchical project explorer with NerdFont file icons,
 * drag & drop support, and xdg-open integration.
 *
 * Start modes:
 *   ase-explorer /path/to/project/  -> Opens given directory as root
 *   ase-explorer                    -> Opens ASE root (/mnt/code/SRC/GITHUB/ase)
 */

#include <gtkmm.h>
#include <giomm.h>
#include <adwaita.h>
#include <explorer/types.hpp>
#include <explorer/version.hpp>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <cstdlib>

namespace fs = std::filesystem;

// ── Generated SSOT headers ─────────────────────────────────────────
// From sha-web-console/generated/ (prebuild pipeline)
#include "file-icons.hpp"

// ── Icon lookup helper ─────────────────────────────────────────────

namespace {

struct ResolvedIcon {
    char32_t glyph;
    double r, g, b;
};

ResolvedIcon unpack_color(const ase::file_icons::FileIcon& icon) {
    uint32_t c = icon.color;
    return {
        icon.glyph,
        ((c >> 16) & 0xFF) / 255.0,
        ((c >>  8) & 0xFF) / 255.0,
        ((c      ) & 0xFF) / 255.0,
    };
}

ResolvedIcon get_file_icon(const std::string& filename) {
    // 1. Exact filename match (highest priority)
    for (int i = 0; i < ase::file_icons::NAME_ICONS_COUNT; ++i) {
        if (filename == ase::file_icons::NAME_ICONS[i].pattern)
            return unpack_color(ase::file_icons::NAME_ICONS[i]);
    }

    // 2. Extension match
    auto dot = filename.rfind('.');
    if (dot != std::string::npos) {
        auto ext = filename.substr(dot);
        // lowercase for comparison
        for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        for (int i = 0; i < ase::file_icons::EXT_ICONS_COUNT; ++i) {
            if (ext == ase::file_icons::EXT_ICONS[i].pattern)
                return unpack_color(ase::file_icons::EXT_ICONS[i]);
        }
    }

    // 3. Fallback
    return unpack_color(ase::file_icons::UNKNOWN);
}

ResolvedIcon get_folder_icon(bool expanded, bool is_submodule) {
    if (is_submodule) return unpack_color(ase::file_icons::SUBMODULE);
    return expanded ? unpack_color(ase::file_icons::FOLDER_OPEN)
                    : unpack_color(ase::file_icons::FOLDER_CLOSED);
}

// Convert a single char32_t to UTF-8 string
std::string to_utf8(char32_t cp) {
    std::string s;
    if (cp < 0x80) {
        s += static_cast<char>(cp);
    } else if (cp < 0x800) {
        s += static_cast<char>(0xC0 | (cp >> 6));
        s += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        s += static_cast<char>(0xE0 | (cp >> 12));
        s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        s += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        s += static_cast<char>(0xF0 | (cp >> 18));
        s += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        s += static_cast<char>(0x80 | (cp & 0x3F));
    }
    return s;
}

// Format Pango markup for an icon glyph with color
std::string icon_markup(const ResolvedIcon& icon) {
    char color_hex[8];
    std::snprintf(color_hex, sizeof(color_hex), "#%02X%02X%02X",
        static_cast<int>(icon.r * 255), static_cast<int>(icon.g * 255), static_cast<int>(icon.b * 255));
    return "<span font='FiraCode Nerd Font 13' foreground='" + std::string(color_hex) + "'>"
         + Glib::Markup::escape_text(to_utf8(icon.glyph)) + "</span>";
}

// ── Exclude patterns ───────────────────────────────────────────────

const std::set<std::string> EXCLUDED_NAMES = {
    "build", "cmake-build-debug", "cmake-build-release", ".cache",
    "node_modules", ".git", ".idea", ".vscode", "__pycache__",
    ".DS_Store", "dist", ".tsbuildinfo",
};

bool should_exclude(const std::string& name) {
    if (EXCLUDED_NAMES.count(name) > 0) return true;
    // Exclude cmake-build-* pattern
    if (name.size() > 12 && name.substr(0, 12) == "cmake-build-") return true;
    return false;
}

// ── VERSION parsing for submodules ─────────────────────────────────

struct SubmoduleInfo {
    int layer = -1;
    std::string status;
    std::string version;
};

// Parse VERSION file in a submodule directory to extract layer, status, version
SubmoduleInfo parse_version_file(const std::string& dir_path) {
    SubmoduleInfo info;
    auto version_path = fs::path(dir_path) / "VERSION";
    if (!fs::exists(version_path)) return info;

    std::ifstream file(version_path);
    std::string line;
    while (std::getline(file, line)) {
        // Match: *_LAYER=N
        if (line.find("_LAYER=") != std::string::npos) {
            auto eq = line.find('=');
            if (eq != std::string::npos) {
                auto val = line.substr(eq + 1);
                while (!val.empty() && (val.back() == '\r' || val.back() == '\n')) val.pop_back();
                info.layer = std::atoi(val.c_str());
            }
        }
        // Match: *_STATUS=xxx
        if (line.find("_STATUS=") != std::string::npos) {
            auto eq = line.find('=');
            if (eq != std::string::npos) {
                info.status = line.substr(eq + 1);
                while (!info.status.empty() && (info.status.back() == '\r' || info.status.back() == '\n'))
                    info.status.pop_back();
            }
        }
        // Match first line: MODULE_NAME=00.05.23.00273 (version number)
        if (line.find('=') != std::string::npos && line.find("_NAME") == std::string::npos
            && line.find("_DESC") == std::string::npos && line.find("_LAYER") == std::string::npos
            && line.find("_STATUS") == std::string::npos && line.find("_CREATED") == std::string::npos
            && line.find("_UPDATED") == std::string::npos && info.version.empty()) {
            auto eq = line.find('=');
            auto val = line.substr(eq + 1);
            while (!val.empty() && (val.back() == '\r' || val.back() == '\n')) val.pop_back();
            // Check if it looks like a version (starts with digit)
            if (!val.empty() && std::isdigit(static_cast<unsigned char>(val[0]))) {
                info.version = val;
            }
        }
    }
    return info;
}

// Status → color (for the status dot in the tree)
uint32_t status_color(const std::string& status) {
    if (status == "stub")   return 0xFF4A4A4A;  // grey
    if (status == "poc")    return 0xFF9C8C4A;  // yellow
    if (status == "init")   return 0xFFB8863A;  // orange
    if (status == "core")   return 0xFF5A9CB8;  // blue
    if (status == "feat")   return 0xFF5A9CB8;  // cyan-ish
    if (status == "refine") return 0xFF4A8C6A;  // green
    if (status == "alpha")  return 0xFF7A5A9C;  // purple
    if (status == "beta")   return 0xFF7A5A9C;  // purple
    if (status == "stable") return 0xFF6A9A5A;  // bright green
    return 0xFF5A5A5A;                          // unknown
}

}  // anonymous namespace

// ── File entry data ────────────────────────────────────────────────

class FileEntry : public Glib::Object {
public:
    std::string name;
    std::string full_path;
    bool is_directory = false;
    bool is_submodule = false;
    int layer = -1;              // L0..L5, -1 = not a submodule
    std::string version_status;  // stub, poc, init, core, feat, refine, alpha, beta, stable
    std::string version_string;  // e.g. "00.05.23.00273"

    static Glib::RefPtr<FileEntry> create(const std::string& name_, const std::string& path_,
                                           bool is_dir, bool is_sub) {
        auto entry = Glib::make_refptr_for_instance(new FileEntry());
        entry->name = name_;
        entry->full_path = path_;
        entry->is_directory = is_dir;
        entry->is_submodule = is_sub;
        return entry;
    }

protected:
    FileEntry() : Glib::Object() {}
};

// ── ExplorerWindow ─────────────────────────────────────────────────

class ExplorerWindow : public Gtk::ApplicationWindow {
public:
    ExplorerWindow() {
        set_title("ASE Explorer");
        set_default_size(ase::explorer::DEFAULT_WIDTH, ase::explorer::DEFAULT_HEIGHT);
        build_ui();
    }

    void load_root(const std::string& path) {
        m_root_path = path;
        parse_submodules();
        populate_root();
        setup_file_monitor();
        update_breadcrumb(path);
        set_title("ASE Explorer \u2014 " + fs::path(path).filename().string());
    }

private:
    std::string m_root_path;
    std::set<std::string> m_submodule_paths;

    Gtk::ListView m_list_view;
    Glib::RefPtr<Gtk::TreeListModel> m_tree_model;
    Glib::RefPtr<Gtk::SingleSelection> m_selection;

    // Phase 5: Live features
    Glib::RefPtr<Gio::FileMonitor> m_file_monitor;
    sigc::connection m_debounce_connection;
    Gtk::SearchEntry* m_search_entry = nullptr;
    Gtk::Box* m_breadcrumb_box = nullptr;
    bool m_search_visible = false;

    // ── UI setup ──

    void build_ui() {
        // HeaderBar
        auto header = Gtk::make_managed<Gtk::HeaderBar>();
        set_titlebar(*header);

        auto title_label = Gtk::make_managed<Gtk::Label>("ASE Explorer");
        title_label->add_css_class("title");
        header->set_title_widget(*title_label);

        // Search entry (hidden by default, toggled with Ctrl+F)
        m_search_entry = Gtk::make_managed<Gtk::SearchEntry>();
        m_search_entry->set_placeholder_text("Filter files...");
        m_search_entry->set_visible(false);
        m_search_entry->signal_search_changed().connect([this]() {
            on_search_changed();
        });
        m_search_entry->signal_stop_search().connect([this]() {
            toggle_search(false);
        });
        header->pack_start(*m_search_entry);

        // Search toggle button
        auto btn_search = Gtk::make_managed<Gtk::Button>();
        btn_search->set_icon_name("system-search-symbolic");
        btn_search->set_tooltip_text("Search (Ctrl+F)");
        btn_search->signal_clicked().connect([this]() { toggle_search(!m_search_visible); });
        header->pack_end(*btn_search);

        // Settings button
        auto btn_settings = Gtk::make_managed<Gtk::Button>();
        btn_settings->set_icon_name("emblem-system-symbolic");
        btn_settings->set_tooltip_text("Settings (Ctrl+,)");
        btn_settings->signal_clicked().connect([this]() { show_settings(); });
        header->pack_end(*btn_settings);

        // Refresh button
        auto btn_refresh = Gtk::make_managed<Gtk::Button>();
        btn_refresh->set_icon_name("view-refresh-symbolic");
        btn_refresh->set_tooltip_text("Refresh (F5)");
        btn_refresh->signal_clicked().connect([this]() { refresh_tree(); });
        header->pack_end(*btn_refresh);

        // Main vertical box: breadcrumb + scrolled tree
        auto vbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 0);
        set_child(*vbox);

        // Breadcrumb path bar
        m_breadcrumb_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 2);
        m_breadcrumb_box->set_margin_start(8);
        m_breadcrumb_box->set_margin_end(8);
        m_breadcrumb_box->set_margin_top(2);
        m_breadcrumb_box->set_margin_bottom(2);
        vbox->append(*m_breadcrumb_box);

        // ScrolledWindow with ListView
        auto scrolled = Gtk::make_managed<Gtk::ScrolledWindow>();
        scrolled->set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
        scrolled->set_vexpand(true);
        scrolled->set_hexpand(true);
        vbox->append(*scrolled);

        // ListView setup (will be populated when root is loaded)
        m_list_view.set_vexpand(true);
        m_list_view.set_hexpand(true);
        scrolled->set_child(m_list_view);

        // Double-click → xdg-open
        auto gesture = Gtk::GestureClick::create();
        gesture->set_button(GDK_BUTTON_PRIMARY);
        gesture->signal_released().connect([this, gesture](int n_press, double, double) {
            if (n_press == 2) on_double_click();
        });
        m_list_view.add_controller(gesture);

        // Right-click → context menu
        auto right_click = Gtk::GestureClick::create();
        right_click->set_button(GDK_BUTTON_SECONDARY);
        right_click->signal_released().connect([this](int, double x, double y) {
            on_right_click(x, y);
        });
        m_list_view.add_controller(right_click);

        // DnD source — provides both text/uri-list and text/plain
        auto drag_source = Gtk::DragSource::create();
        drag_source->set_actions(Gdk::DragAction::COPY);
        drag_source->signal_prepare().connect(
            [this](double, double) -> Glib::RefPtr<Gdk::ContentProvider> {
                auto entry = get_selected_entry();
                if (!entry) return {};

                // text/uri-list format (standard DnD protocol for file managers)
                auto file = Gio::File::create_for_path(entry->full_path);
                auto uri_value = Glib::Value<Glib::ustring>();
                uri_value.init(uri_value.value_type());
                uri_value.set(file->get_uri() + "\r\n");

                // text/plain format (for terminals, chat apps, etc.)
                auto text_value = Glib::Value<Glib::ustring>();
                text_value.init(text_value.value_type());
                text_value.set(entry->full_path);

                // Combine both providers
                std::vector<Glib::RefPtr<Gdk::ContentProvider>> providers;
                providers.push_back(Gdk::ContentProvider::create(uri_value));
                providers.push_back(Gdk::ContentProvider::create(text_value));
                return Gdk::ContentProvider::create(providers);
            }, false);
        m_list_view.add_controller(drag_source);

        // Keyboard shortcuts
        auto key_ctrl = Gtk::EventControllerKey::create();
        key_ctrl->signal_key_pressed().connect(
            [this](guint keyval, guint, Gdk::ModifierType state) -> bool {
                bool ctrl = (state & Gdk::ModifierType::CONTROL_MASK) != Gdk::ModifierType();
                if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
                    on_double_click();
                    return true;
                }
                if (ctrl && (keyval == GDK_KEY_c || keyval == GDK_KEY_C)) {
                    copy_path_to_clipboard(
                        (state & Gdk::ModifierType::SHIFT_MASK) != Gdk::ModifierType());
                    return true;
                }
                if (ctrl && keyval == GDK_KEY_f) {
                    toggle_search(!m_search_visible);
                    return true;
                }
                if (ctrl && keyval == GDK_KEY_comma) {
                    show_settings();
                    return true;
                }
                if (keyval == GDK_KEY_F5) {
                    refresh_tree();
                    return true;
                }
                if (keyval == GDK_KEY_Escape) {
                    if (m_search_visible) { toggle_search(false); return true; }
                    return false;
                }
                return false;
            }, false);
        add_controller(key_ctrl);
    }

    // ── Submodule detection ──

    void parse_submodules() {
        m_submodule_paths.clear();
        auto gitmodules = fs::path(m_root_path) / ".gitmodules";
        if (!fs::exists(gitmodules)) return;

        std::ifstream file(gitmodules);
        std::string line;
        while (std::getline(file, line)) {
            // Match: path = some/path
            auto pos = line.find("path = ");
            if (pos != std::string::npos) {
                auto rel = line.substr(pos + 7);
                // Trim whitespace
                while (!rel.empty() && (rel.back() == '\r' || rel.back() == '\n' || rel.back() == ' '))
                    rel.pop_back();
                auto abs = (fs::path(m_root_path) / rel).string();
                m_submodule_paths.insert(abs);
            }
        }
    }

    // ── Tree model ──

    Glib::RefPtr<Gio::ListModel> create_child_model(const Glib::RefPtr<Glib::ObjectBase>& item) {
        auto row = std::dynamic_pointer_cast<Gtk::TreeListRow>(item);
        if (!row) return {};
        auto entry = std::dynamic_pointer_cast<FileEntry>(row->get_item());
        if (!entry || !entry->is_directory) return {};
        return scan_directory(entry->full_path);
    }

    Glib::RefPtr<Gio::ListStore<FileEntry>> scan_directory(const std::string& dir_path) {
        auto store = Gio::ListStore<FileEntry>::create();

        if (!fs::exists(dir_path) || !fs::is_directory(dir_path)) return store;

        struct Entry { std::string name; std::string path; bool is_dir; bool is_sub; };
        std::vector<Entry> entries;

        for (const auto& e : fs::directory_iterator(dir_path, fs::directory_options::skip_permission_denied)) {
            auto name = e.path().filename().string();
            if (name.empty()) continue;
            if (should_exclude(name)) continue;

            bool is_dir = e.is_directory();
            bool is_sub = m_submodule_paths.count(e.path().string()) > 0;
            entries.push_back({name, e.path().string(), is_dir, is_sub});
        }

        // Sort: directories first, then alphabetical (case-insensitive)
        std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
            if (a.is_dir != b.is_dir) return a.is_dir > b.is_dir;
            auto la = a.name, lb = b.name;
            std::transform(la.begin(), la.end(), la.begin(), ::tolower);
            std::transform(lb.begin(), lb.end(), lb.begin(), ::tolower);
            return la < lb;
        });

        for (const auto& e : entries) {
            auto fe = FileEntry::create(e.name, e.path, e.is_dir, e.is_sub);
            // Parse VERSION for submodules to get layer + status
            if (e.is_sub) {
                auto info = parse_version_file(e.path);
                fe->layer = info.layer;
                fe->version_status = info.status;
                fe->version_string = info.version;
            }
            store->append(fe);
        }

        return store;
    }

    void populate_root() {
        auto root_store = scan_directory(m_root_path);

        m_tree_model = Gtk::TreeListModel::create(
            root_store,
            false, // passthrough = false (we wrap items in TreeListRow)
            true,  // autoexpand = false initially
            sigc::mem_fun(*this, &ExplorerWindow::create_child_model_slot));

        m_selection = Gtk::SingleSelection::create(m_tree_model);
        m_selection->set_autoselect(false);

        // Factory for rendering rows
        auto factory = Gtk::SignalListItemFactory::create();
        factory->signal_setup().connect([](const Glib::RefPtr<Gtk::ListItem>& item) {
            auto box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
            auto expander = Gtk::make_managed<Gtk::TreeExpander>();
            auto icon_label = Gtk::make_managed<Gtk::Label>();
            auto name_label = Gtk::make_managed<Gtk::Label>();

            icon_label->set_use_markup(true);
            name_label->set_xalign(0.0f);
            name_label->set_ellipsize(Pango::EllipsizeMode::END);
            name_label->set_hexpand(true);

            // badge_label shows [L3 core] for submodules
            auto badge_label = Gtk::make_managed<Gtk::Label>();
            badge_label->set_use_markup(true);
            badge_label->set_xalign(1.0f);

            auto inner_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 4);
            inner_box->append(*icon_label);
            inner_box->append(*name_label);
            inner_box->append(*badge_label);

            expander->set_child(*inner_box);
            box->append(*expander);
            item->set_child(*box);
        });

        factory->signal_bind().connect([](const Glib::RefPtr<Gtk::ListItem>& item) {
            auto box = dynamic_cast<Gtk::Box*>(item->get_child());
            if (!box) return;
            auto expander = dynamic_cast<Gtk::TreeExpander*>(box->get_first_child());
            if (!expander) return;
            auto inner_box = dynamic_cast<Gtk::Box*>(expander->get_child());
            if (!inner_box) return;
            auto icon_label = dynamic_cast<Gtk::Label*>(inner_box->get_first_child());
            auto name_label = dynamic_cast<Gtk::Label*>(icon_label ? icon_label->get_next_sibling() : nullptr);
            auto badge_label = dynamic_cast<Gtk::Label*>(name_label ? name_label->get_next_sibling() : nullptr);
            if (!icon_label || !name_label) return;

            auto row = std::dynamic_pointer_cast<Gtk::TreeListRow>(item->get_item());
            if (!row) return;
            expander->set_list_row(row);

            auto entry = std::dynamic_pointer_cast<FileEntry>(row->get_item());
            if (!entry) return;

            name_label->set_text(entry->name);

            // Set icon
            ResolvedIcon ri;
            if (entry->is_directory) {
                ri = get_folder_icon(row->get_expanded(), entry->is_submodule);
            } else {
                ri = get_file_icon(entry->name);
            }
            icon_label->set_markup(icon_markup(ri));

            // Badge for submodules: [L3 core] with status-colored dot
            if (badge_label) {
                if (entry->is_submodule && !entry->version_status.empty()) {
                    uint32_t sc = status_color(entry->version_status);
                    char hex[8];
                    std::snprintf(hex, sizeof(hex), "#%02X%02X%02X",
                        (sc >> 16) & 0xFF, (sc >> 8) & 0xFF, sc & 0xFF);
                    std::string badge = "<span font='8' foreground='" + std::string(hex) + "'>\u25CF</span>"
                        " <span font='Fira Code 8' foreground='#5A5A5A'>[";
                    if (entry->layer >= 0) badge += "L" + std::to_string(entry->layer) + " ";
                    badge += entry->version_status + "]</span>";
                    badge_label->set_markup(badge);
                    // Tooltip: full version info
                    if (!entry->version_string.empty()) {
                        badge_label->set_tooltip_text(
                            entry->name + " v" + entry->version_string + " [" + entry->version_status + "]");
                    }
                } else {
                    badge_label->set_markup("");
                    badge_label->set_tooltip_text("");
                }
            }
        });

        m_list_view.set_model(m_selection);
        m_list_view.set_factory(factory);
    }

    Glib::RefPtr<Gio::ListModel> create_child_model_slot(const Glib::RefPtr<Glib::ObjectBase>& item) {
        auto entry = std::dynamic_pointer_cast<FileEntry>(item);
        if (!entry || !entry->is_directory) return {};
        return scan_directory(entry->full_path);
    }

    // ── Interaction ──

    Glib::RefPtr<FileEntry> get_selected_entry() {
        if (!m_selection) return {};
        auto pos = m_selection->get_selected();
        if (pos == GTK_INVALID_LIST_POSITION) return {};
        auto row = std::dynamic_pointer_cast<Gtk::TreeListRow>(m_selection->get_selected_item());
        if (!row) return {};
        return std::dynamic_pointer_cast<FileEntry>(row->get_item());
    }

    void on_double_click() {
        auto entry = get_selected_entry();
        if (!entry) return;

        if (entry->is_directory) {
            // Toggle expand/collapse — handled by TreeExpander
            return;
        }

        // Open file with default application
        auto file = Gio::File::create_for_path(entry->full_path);
        auto launcher = Gtk::FileLauncher::create(file);
        launcher->launch(dynamic_cast<Gtk::Window&>(*this), nullptr);
    }

    void on_right_click(double x, double y) {
        auto entry = get_selected_entry();
        if (!entry) return;

        auto menu = Gio::Menu::create();
        menu->append("Open", "explorer.open");
        menu->append("Open With...", "explorer.open-with");
        menu->append("Copy Path", "explorer.copy-path");
        menu->append("Copy Relative Path", "explorer.copy-rel-path");
        menu->append("Open in Terminal", "explorer.open-terminal");
        menu->append("Reveal in File Manager", "explorer.reveal");

        // Actions
        auto group = Gio::SimpleActionGroup::create();
        group->add_action("open", [this]() { on_double_click(); });
        group->add_action("open-with", [this, entry]() {
            auto file = Gio::File::create_for_path(entry->full_path);
            auto launcher = Gtk::FileLauncher::create(file);
            launcher->open_containing_folder(dynamic_cast<Gtk::Window&>(*this), nullptr);
        });
        group->add_action("copy-path", [this]() { copy_path_to_clipboard(false); });
        group->add_action("copy-rel-path", [this]() { copy_path_to_clipboard(true); });
        group->add_action("open-terminal", [this, entry]() {
            auto dir = entry->is_directory ? entry->full_path
                                           : fs::path(entry->full_path).parent_path().string();
            auto cmd = "foot --working-directory=" + dir;
            Glib::spawn_command_line_async(cmd);
        });
        group->add_action("reveal", [this, entry]() {
            auto dir = entry->is_directory ? entry->full_path
                                           : fs::path(entry->full_path).parent_path().string();
            auto file = Gio::File::create_for_path(dir);
            auto launcher = Gtk::FileLauncher::create(file);
            launcher->launch(dynamic_cast<Gtk::Window&>(*this), nullptr);
        });
        m_list_view.insert_action_group("explorer", group);

        auto popover = Gtk::make_managed<Gtk::PopoverMenu>(menu);
        popover->set_parent(m_list_view);
        popover->set_pointing_to(Gdk::Rectangle(static_cast<int>(x), static_cast<int>(y), 1, 1));
        popover->popup();
    }

    void copy_path_to_clipboard(bool relative) {
        auto entry = get_selected_entry();
        if (!entry) return;

        std::string path = entry->full_path;
        if (relative && !m_root_path.empty()) {
            path = fs::relative(entry->full_path, m_root_path).string();
        }

        get_clipboard()->set_text(path);
    }

    // ── Phase 5: Live Features ──

    void setup_file_monitor() {
        auto root_file = Gio::File::create_for_path(m_root_path);
        m_file_monitor = root_file->monitor_directory(Gio::FileMonitor::Flags::WATCH_MOVES);
        m_file_monitor->signal_changed().connect(
            [this](const Glib::RefPtr<Gio::File>&, const Glib::RefPtr<Gio::File>&,
                   Gio::FileMonitor::Event) {
                // Debounce: wait 500ms before refreshing to batch rapid changes
                if (m_debounce_connection.connected()) m_debounce_connection.disconnect();
                m_debounce_connection = Glib::signal_timeout().connect([this]() {
                    refresh_tree();
                    return false;  // one-shot
                }, 500);
            });
    }

    void refresh_tree() {
        if (m_root_path.empty()) return;
        parse_submodules();
        populate_root();
    }

    void toggle_search(bool visible) {
        m_search_visible = visible;
        m_search_entry->set_visible(visible);
        if (visible) {
            m_search_entry->grab_focus();
        } else {
            m_search_entry->set_text("");
        }
    }

    void on_search_changed() {
        auto text = m_search_entry->get_text();
        if (text.empty()) {
            // Reset: reload full tree
            refresh_tree();
            return;
        }

        // Filter: rebuild tree showing only matching entries
        // For now, a simple approach: rebuild with filter text
        // Full fuzzy-match would require a custom Gtk::Filter on the model
        auto filter_text = text.lowercase();
        (void)filter_text;
        // TODO(Phase 5.2): implement Gtk::CustomFilter on TreeListModel
        // For now the search entry is wired up but filtering is deferred
        // until the base tree renders correctly after build verification
    }

    void update_breadcrumb(const std::string& path) {
        if (!m_breadcrumb_box) return;

        // Clear existing breadcrumbs
        while (auto child = m_breadcrumb_box->get_first_child()) {
            m_breadcrumb_box->remove(*child);
        }

        // Build path components
        auto rel = fs::relative(path, "/mnt/code/SRC/GITHUB");
        auto components = std::vector<std::pair<std::string, std::string>>();
        auto current = fs::path(path);
        auto base = fs::path("/mnt/code/SRC/GITHUB");

        // Collect components from root to current
        for (auto it = rel.begin(); it != rel.end(); ++it) {
            base = base / *it;
            components.push_back({it->string(), base.string()});
        }

        for (size_t i = 0; i < components.size(); ++i) {
            if (i > 0) {
                auto sep = Gtk::make_managed<Gtk::Label>("/");
                sep->add_css_class("dim-label");
                m_breadcrumb_box->append(*sep);
            }

            auto btn = Gtk::make_managed<Gtk::Button>(components[i].first);
            btn->add_css_class("flat");
            btn->add_css_class("dim-label");
            auto target_path = components[i].second;
            btn->signal_clicked().connect([this, target_path]() {
                load_root(target_path);
            });
            m_breadcrumb_box->append(*btn);
        }

        // Copy button at the end
        auto copy_btn = Gtk::make_managed<Gtk::Button>();
        copy_btn->set_icon_name("edit-copy-symbolic");
        copy_btn->set_tooltip_text("Copy path");
        copy_btn->add_css_class("flat");
        auto p = path;
        copy_btn->signal_clicked().connect([this, p]() {
            get_clipboard()->set_text(p);
        });
        m_breadcrumb_box->append(*copy_btn);
    }

    // ── Phase 6: Settings ──

    void show_settings() {
        // Use libadwaita C API directly (no gtkmm wrapper available)
        auto* prefs = adw_preferences_window_new();
        gtk_window_set_transient_for(GTK_WINDOW(prefs), GTK_WINDOW(this->gobj()));
        gtk_window_set_modal(GTK_WINDOW(prefs), TRUE);

        // General page
        auto* page_general = adw_preferences_page_new();
        adw_preferences_page_set_title(ADW_PREFERENCES_PAGE(page_general), "General");
        adw_preferences_page_set_icon_name(ADW_PREFERENCES_PAGE(page_general), "preferences-system-symbolic");

        auto* group_general = adw_preferences_group_new();
        adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(group_general), "File Display");

        auto* row_hidden = adw_switch_row_new();
        adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row_hidden), "Show Hidden Files");
        adw_switch_row_set_active(ADW_SWITCH_ROW(row_hidden), FALSE);
        adw_preferences_group_add(ADW_PREFERENCES_GROUP(group_general), GTK_WIDGET(row_hidden));

        auto* row_gitignored = adw_switch_row_new();
        adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row_gitignored), "Show .gitignore'd Files");
        adw_switch_row_set_active(ADW_SWITCH_ROW(row_gitignored), FALSE);
        adw_preferences_group_add(ADW_PREFERENCES_GROUP(group_general), GTK_WIDGET(row_gitignored));

        adw_preferences_page_add(ADW_PREFERENCES_PAGE(page_general), ADW_PREFERENCES_GROUP(group_general));
        adw_preferences_window_add(ADW_PREFERENCES_WINDOW(prefs), ADW_PREFERENCES_PAGE(page_general));

        // Appearance page
        auto* page_appearance = adw_preferences_page_new();
        adw_preferences_page_set_title(ADW_PREFERENCES_PAGE(page_appearance), "Appearance");
        adw_preferences_page_set_icon_name(ADW_PREFERENCES_PAGE(page_appearance), "applications-graphics-symbolic");

        auto* group_theme = adw_preferences_group_new();
        adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(group_theme), "Theme");

        auto* row_compact = adw_switch_row_new();
        adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row_compact), "Compact Mode");
        adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row_compact), "Compact Mode");
        adw_switch_row_set_active(ADW_SWITCH_ROW(row_compact), FALSE);
        adw_preferences_group_add(ADW_PREFERENCES_GROUP(group_theme), GTK_WIDGET(row_compact));

        adw_preferences_page_add(ADW_PREFERENCES_PAGE(page_appearance), ADW_PREFERENCES_GROUP(group_theme));
        adw_preferences_window_add(ADW_PREFERENCES_WINDOW(prefs), ADW_PREFERENCES_PAGE(page_appearance));

        // Behavior page
        auto* page_behavior = adw_preferences_page_new();
        adw_preferences_page_set_title(ADW_PREFERENCES_PAGE(page_behavior), "Behavior");
        adw_preferences_page_set_icon_name(ADW_PREFERENCES_PAGE(page_behavior), "preferences-other-symbolic");

        auto* group_behavior = adw_preferences_group_new();
        adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(group_behavior), "Interaction");

        auto* row_singleclick = adw_switch_row_new();
        adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row_singleclick), "Single-Click Open");
        adw_switch_row_set_active(ADW_SWITCH_ROW(row_singleclick), FALSE);
        adw_preferences_group_add(ADW_PREFERENCES_GROUP(group_behavior), GTK_WIDGET(row_singleclick));

        auto* row_filewatch = adw_switch_row_new();
        adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row_filewatch), "Live File Watching");
        adw_switch_row_set_active(ADW_SWITCH_ROW(row_filewatch), TRUE);
        adw_preferences_group_add(ADW_PREFERENCES_GROUP(group_behavior), GTK_WIDGET(row_filewatch));

        auto* row_terminal = adw_entry_row_new();
        adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row_terminal), "Terminal Emulator");
        gtk_editable_set_text(GTK_EDITABLE(row_terminal), "foot");
        adw_preferences_group_add(ADW_PREFERENCES_GROUP(group_behavior), GTK_WIDGET(row_terminal));

        adw_preferences_page_add(ADW_PREFERENCES_PAGE(page_behavior), ADW_PREFERENCES_GROUP(group_behavior));
        adw_preferences_window_add(ADW_PREFERENCES_WINDOW(prefs), ADW_PREFERENCES_PAGE(page_behavior));

        gtk_window_present(GTK_WINDOW(prefs));
    }
};

// ── ExplorerApp ────────────────────────────────────────────────────

class ExplorerApp : public Gtk::Application {
public:
    ExplorerApp()
        : Gtk::Application("com.antarien.ase.explorer",
                           Gio::Application::Flags::HANDLES_OPEN) {}

protected:
    void on_startup() override {
        Gtk::Application::on_startup();
        adw_init();

        // Force dark color scheme via libadwaita
        adw_style_manager_set_color_scheme(
            adw_style_manager_get_default(), ADW_COLOR_SCHEME_FORCE_DARK);

        // Dark theme CSS
        auto css = Gtk::CssProvider::create();
        css->load_from_data(R"(
            window {
                background-color: #040404;
                color: #8A9A9A;
            }
            headerbar {
                background: linear-gradient(to right, #0A0A0A, #121212);
                border-bottom: 1px solid rgba(255, 255, 255, 0.03);
                color: #5a9cb8;
                min-height: 32px;
            }
            listview {
                background-color: transparent;
                color: #8A9A9A;
                font-family: 'Fira Code', monospace;
                font-size: 12px;
            }
            listview > row {
                padding: 1px 4px;
                min-height: 22px;
            }
            listview > row:hover {
                background-color: #151515;
            }
            listview > row:selected {
                background-color: #121212;
                color: #5a9cb8;
            }
            treeexpander {
                min-width: 16px;
            }
        )");
        Gtk::StyleContext::add_provider_for_display(
            Gdk::Display::get_default(), css,
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

        auto settings = Gtk::Settings::get_default();
        settings->property_gtk_application_prefer_dark_theme() = true;
    }

    void on_activate() override {
        auto window = create_window();
        window->load_root(ase::explorer::DEFAULT_ROOT);
        window->present();
    }

    void on_open(const Gio::Application::type_vec_files& files,
                 const Glib::ustring& hint) override {
        (void)hint;
        auto window = create_window();

        if (!files.empty()) {
            auto path = files[0]->get_path();
            if (fs::is_directory(path)) {
                window->load_root(path);
            } else {
                // If given a file, open its parent directory
                window->load_root(fs::path(path).parent_path().string());
            }
        } else {
            window->load_root(ase::explorer::DEFAULT_ROOT);
        }

        window->present();
    }

private:
    ExplorerWindow* create_window() {
        auto window = new ExplorerWindow();
        add_window(*window);
        return window;
    }
};

// ── Entry point ────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    auto app = ExplorerApp::create();
    return app->run(argc, argv);
}

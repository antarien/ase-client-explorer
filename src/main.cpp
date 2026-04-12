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
#include <explorer/types.hpp>
#include <explorer/version.hpp>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <string>
#include <vector>
#include <set>

namespace fs = std::filesystem;

// ── Generated SSOT headers ─────────────────────────────────────────

// Inline the file-icons data directly (constexpr, header-only)
// Path: sha-web-console/generated/file-icons.hpp (prebuild rsync'd)
namespace ase::file_icons {

struct FileIcon {
    const char* pattern;
    char32_t    glyph;
    uint32_t    color;      // 0xAARRGGBB
    bool        exact_name;
};

// ── Special icons ──
constexpr FileIcon FOLDER_CLOSED = {"", U'\xF07B', 0xFF9C8C4A, false};
constexpr FileIcon FOLDER_OPEN   = {"", U'\xF07C', 0xFF9C8C4A, false};
constexpr FileIcon SUBMODULE     = {"", U'\xF126', 0xFF5A9CB8, false};
constexpr FileIcon UNKNOWN       = {"", U'\xF016', 0xFF5A5A5A, false};

// ── Extension-based icons ──
constexpr FileIcon EXT_ICONS[] = {
    {".cpp",  U'\xE61D', 0xFF5A9CB8, false}, {".cxx",  U'\xE61D', 0xFF5A9CB8, false},
    {".cc",   U'\xE61D', 0xFF5A9CB8, false}, {".hpp",  U'\xE61D', 0xFF7A5A9C, false},
    {".hxx",  U'\xE61D', 0xFF7A5A9C, false}, {".h",    U'\xE61E', 0xFF7A5A9C, false},
    {".c",    U'\xE61E', 0xFF5A9CB8, false}, {".inl",  U'\xE61D', 0xFF7A5A9C, false},
    {".ts",   U'\xE8CA', 0xFF5A9CB8, false}, {".tsx",  U'\xE7BA', 0xFF5A9CB8, false},
    {".js",   U'\xE781', 0xFF9C8C4A, false}, {".jsx",  U'\xE7BA', 0xFF9C8C4A, false},
    {".mjs",  U'\xE781', 0xFF9C8C4A, false}, {".cjs",  U'\xE781', 0xFF9C8C4A, false},
    {".py",   U'\xE73C', 0xFF4A8C6A, false}, {".rs",   U'\xE7A8', 0xFFB8863A, false},
    {".go",   U'\xE724', 0xFF5A9CB8, false}, {".java", U'\xE738', 0xFFA84A4A, false},
    {".kt",   U'\xE81B', 0xFF7A5A9C, false}, {".swift",U'\xE755', 0xFFB8863A, false},
    {".rb",   U'\xE739', 0xFFA84A4A, false}, {".php",  U'\xE73D', 0xFF7A5A9C, false},
    {".lua",  U'\xF121', 0xFF5A9CB8, false}, {".html", U'\xF121', 0xFFB8863A, false},
    {".css",  U'\xF13C', 0xFF5A9CB8, false}, {".scss", U'\xF13C', 0xFF7A5A9C, false},
    {".json", U'\xF085', 0xFF9C8C4A, false}, {".yaml", U'\xF1DE', 0xFF7A5A9C, false},
    {".yml",  U'\xF1DE', 0xFF7A5A9C, false}, {".toml", U'\xF085', 0xFFB8863A, false},
    {".md",   U'\xF15C', 0xFF8A9A9A, false}, {".txt",  U'\xF0F6', 0xFF6A7A7A, false},
    {".pdf",  U'\xF1C1', 0xFFA84A4A, false}, {".sh",   U'\xE795', 0xFF4A8C6A, false},
    {".bash", U'\xE795', 0xFF4A8C6A, false}, {".zsh",  U'\xE795', 0xFF4A8C6A, false},
    {".cmake",U'\xEEFF', 0xFF4A8C6A, false}, {".svg",  U'\xF03E', 0xFF9C8C4A, false},
    {".png",  U'\xF03E', 0xFF7A5A9C, false}, {".jpg",  U'\xF03E', 0xFF7A5A9C, false},
    {".glsl", U'\xF0E7', 0xFF9C8C4A, false}, {".vert", U'\xF0E7', 0xFF5A9CB8, false},
    {".frag", U'\xF0E7', 0xFFB8863A, false}, {".sql",  U'\xF1C0', 0xFF5A9CB8, false},
    {".env",  U'\xF023', 0xFFA84A4A, false}, {".lock", U'\xF023', 0xFF4A4A4A, false},
    {".log",  U'\xF0F6', 0xFF4A4A4A, false}, {".wasm", U'\xF1B2', 0xFF7A5A9C, false},
    {".xml",  U'\xF121', 0xFFB8863A, false}, {".ini",  U'\xF1DE', 0xFF9C8C4A, false},
    {".csv",  U'\xF0CE', 0xFF4A8C6A, false}, {".zip",  U'\xF1C6', 0xFFB8863A, false},
    {".tar",  U'\xF1C6', 0xFFB8863A, false}, {".gz",   U'\xF1C6', 0xFFB8863A, false},
};
constexpr int EXT_ICONS_COUNT = sizeof(EXT_ICONS) / sizeof(EXT_ICONS[0]);

// ── Exact filename icons ──
constexpr FileIcon NAME_ICONS[] = {
    {"CMakeLists.txt",   U'\xEEFF', 0xFF4A8C6A, true},
    {"Makefile",         U'\xEEFF', 0xFF4A8C6A, true},
    {"Dockerfile",       U'\xE7B0', 0xFF5A9CB8, true},
    {"VERSION",          U'\xF02B', 0xFF9C8C4A, true},
    {"README.md",        U'\xF02D', 0xFF5A9CB8, true},
    {"CLAUDE.md",        U'\xEE0D', 0xFF7A5A9C, true},
    {"LICENSE",          U'\xF0A3', 0xFF9C8C4A, true},
    {"package.json",     U'\xE71E', 0xFFA84A4A, true},
    {"package-lock.json",U'\xE71E', 0xFF4A4A4A, true},
    {"tsconfig.json",    U'\xE8CA', 0xFF5A9CB8, true},
    {"vite.config.ts",   U'\xF0E7', 0xFF9C8C4A, true},
    {".gitignore",       U'\xF1D3', 0xFFB8863A, true},
    {".gitmodules",      U'\xF1D3', 0xFFB8863A, true},
    {"build.sh",         U'\xF135', 0xFF4A8C6A, true},
    {"PKGBUILD",         U'\xF187', 0xFF5A9CB8, true},
};
constexpr int NAME_ICONS_COUNT = sizeof(NAME_ICONS) / sizeof(NAME_ICONS[0]);

}  // namespace ase::file_icons

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

const std::set<std::string> EXCLUDED_DIRS = {
    "build", "cmake-build-debug", ".cache", "node_modules", ".git",
    ".idea", ".vscode", "__pycache__", ".DS_Store", "cmake-build-release",
};

bool should_exclude(const std::string& name) {
    return EXCLUDED_DIRS.count(name) > 0;
}

}  // anonymous namespace

// ── File entry data ────────────────────────────────────────────────

class FileEntry : public Glib::Object {
public:
    std::string name;
    std::string full_path;
    bool is_directory = false;
    bool is_submodule = false;

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
        set_title("ASE Explorer \u2014 " + fs::path(path).filename().string());
    }

private:
    std::string m_root_path;
    std::set<std::string> m_submodule_paths;

    Gtk::ListView m_list_view;
    Glib::RefPtr<Gtk::TreeListModel> m_tree_model;
    Glib::RefPtr<Gtk::SingleSelection> m_selection;

    // ── UI setup ──

    void build_ui() {
        // HeaderBar
        auto header = Gtk::make_managed<Gtk::HeaderBar>();
        set_titlebar(*header);

        auto title_label = Gtk::make_managed<Gtk::Label>("ASE Explorer");
        title_label->add_css_class("title");
        header->set_title_widget(*title_label);

        // Search button (placeholder for Phase 5)
        auto btn_search = Gtk::make_managed<Gtk::Button>();
        btn_search->set_icon_name("system-search-symbolic");
        btn_search->set_tooltip_text("Search (Ctrl+F)");
        header->pack_end(*btn_search);

        // Settings button (placeholder for Phase 6)
        auto btn_settings = Gtk::make_managed<Gtk::Button>();
        btn_settings->set_icon_name("emblem-system-symbolic");
        btn_settings->set_tooltip_text("Settings (Ctrl+,)");
        header->pack_end(*btn_settings);

        // Main content: ScrolledWindow with ListView
        auto scrolled = Gtk::make_managed<Gtk::ScrolledWindow>();
        scrolled->set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
        scrolled->set_vexpand(true);
        scrolled->set_hexpand(true);
        set_child(*scrolled);

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

        // DnD source
        auto drag_source = Gtk::DragSource::create();
        drag_source->set_actions(Gdk::DragAction::COPY);
        drag_source->signal_prepare().connect(
            [this](double, double) -> Glib::RefPtr<Gdk::ContentProvider> {
                auto entry = get_selected_entry();
                if (!entry) return {};
                auto uri = "file://" + entry->full_path;
                auto value = Glib::Value<Glib::ustring>();
                value.init(value.value_type());
                value.set(uri);
                return Gdk::ContentProvider::create(value);
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
            if (name.empty() || name[0] == '.') continue;
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
            store->append(FileEntry::create(e.name, e.path, e.is_dir, e.is_sub));
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

            auto inner_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 4);
            inner_box->append(*icon_label);
            inner_box->append(*name_label);

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
        menu->append("Copy Path", "explorer.copy-path");
        menu->append("Copy Relative Path", "explorer.copy-rel-path");
        menu->append("Open in Terminal", "explorer.open-terminal");

        // Actions
        auto group = Gio::SimpleActionGroup::create();
        group->add_action("open", [this]() { on_double_click(); });
        group->add_action("copy-path", [this]() { copy_path_to_clipboard(false); });
        group->add_action("copy-rel-path", [this]() { copy_path_to_clipboard(true); });
        group->add_action("open-terminal", [this, entry]() {
            auto dir = entry->is_directory ? entry->full_path
                                           : fs::path(entry->full_path).parent_path().string();
            auto cmd = "foot --working-directory=" + dir;
            Glib::spawn_command_line_async(cmd);
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

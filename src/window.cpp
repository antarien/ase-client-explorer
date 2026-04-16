/**
 * @file        window.cpp
 * @brief       Implementation for window.hpp
 * @description build_ui() lays out header + breadcrumb + scrolled tree view,
 *              installs every feature slice, and wires their slots back to
 *              this->handle_* methods. load_root() and refresh() are the two
 *              state-mutating entry points everyone else routes through.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <explorer/window.hpp>

#include <explorer/app_catalog.hpp>
#include <explorer/clipboard_ops.hpp>
#include <explorer/icons.hpp>
#include <explorer/open_with_dialog.hpp>
#include <explorer/settings_dialog.hpp>
#include <explorer/types.hpp>

#include "colors.hpp"
#include "ui_icons.hpp"

#include <ase/adp/gtk/gesture.hpp>
#include <ase/adp/gtk/io.hpp>
#include <ase/adp/gtk/widget.hpp>
#include <ase/utils/fs.hpp>

#include <giomm/file.h>
#include <gdkmm/contentprovider.h>
#include <glibmm/value.h>
#include <glibmm/ustring.h>
#include <glibmm/wrap.h>

#include <gdk/gdk.h>
#include <gio/gio.h>
#include <glib.h>
#include <gtk/gtk.h>

#include <memory>
#include <utility>
#include <vector>

namespace ase::explorer {

ExplorerWindow::ExplorerWindow(ase::adp::gtk::ApplicationWindow window)
    : m_window(std::move(window))
{}

void ExplorerWindow::build_ui() {
    m_window.set_default_size(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    m_window.set_title("ASE Explorer");

    // ── Header bar ──
    auto header = ase::adp::gtk::HeaderBar::create();
    header.set_show_title_buttons(false);

    auto title_label = ase::adp::gtk::Label::create("ASE Explorer");
    title_label.add_css_class("title");
    header.pack_start(title_label);
    m_title_label_native = title_label.native_widget()
        ? title_label.native_widget()->gobj() : nullptr;

    // Search entry sits in the center title slot. Initially invisible
    // (handled by SearchBar). When the user toggles search on, the title
    // label hides and the search entry expands to fill the entire header
    // width — see handle_search_toggle().
    header.set_title_widget(m_search_bar.entry());
    m_search_bar.entry().set_hexpand(true);

    // Header-bar glyph buttons. Every glyph traces back to the SSOT
    // (icon-definitions.ts → ui_icons.hpp). gnome-icon-theme symbolic
    // names are FORBIDDEN in this client per INST_ASE_ICO_SYS.md.
    constexpr uint32_t header_glyph_color = ase::colors::TEXT_LIGHT & 0xFFFFFF;
    auto set_glyph = [](ase::adp::gtk::Button& b, char32_t g) {
        GtkWidget* lbl = ase::explorer::icons::make_glyph_label(
            g, header_glyph_color, ase::explorer::icons::ICON_FONT_SIZE);
        gtk_button_set_child(GTK_BUTTON(b.native()->gobj()), lbl);
    };

    auto btn_settings = ase::adp::gtk::Button::create();
    set_glyph(btn_settings, ase::ui_icons::ICON_SETTINGS);
    btn_settings.set_tooltip_text("Settings (Ctrl+,)");
    btn_settings.on_clicked([this]() { settings_dialog::show(m_window, m_file_associations, m_settings, m_root_path,
                              [this]() {
                                  m_breadcrumb.set_max_segments(m_settings.breadcrumb_max_segments());
                                  // If the user changed the default root, jump to it now;
                                  // otherwise just refresh so newly-mapped extensions show.
                                  if (m_settings.default_root() != m_root_path) {
                                      load_root(m_settings.default_root());
                                  } else {
                                      refresh();
                                  }
                              }); });
    header.pack_end(btn_settings);

    auto btn_refresh = ase::adp::gtk::Button::create();
    set_glyph(btn_refresh, ase::ui_icons::ICON_REFRESH);
    btn_refresh.set_tooltip_text("Refresh (F5)");
    btn_refresh.on_clicked([this]() { refresh(); });
    header.pack_end(btn_refresh);

    auto btn_search = ase::adp::gtk::Button::create();
    set_glyph(btn_search, ase::ui_icons::ICON_SEARCH);
    btn_search.set_tooltip_text("Search (Ctrl+F)");
    btn_search.on_clicked([this]() { handle_search_toggle(); });
    header.pack_end(btn_search);

    auto btn_expand = ase::adp::gtk::Button::create();
    set_glyph(btn_expand, ase::ui_icons::ICON_CARET_DOWN);
    btn_expand.set_tooltip_text("Expand / collapse selected item");
    btn_expand.on_clicked([this]() {
        m_tree_view.toggle_recursive_expand_selected();
    });
    header.pack_end(btn_expand);

    auto btn_copy = ase::adp::gtk::Button::create();
    set_glyph(btn_copy, ase::ui_icons::ICON_COPY);
    btn_copy.set_tooltip_text("Copy current path");
    btn_copy.on_clicked([this]() {
        if (!m_root_path.empty()) {
            ase::adp::gtk::copy_to_clipboard(m_tree_view.list_view(), m_root_path);
        }
    });
    header.pack_end(btn_copy);

    m_window.set_titlebar(header);

    // ── Main vertical layout: breadcrumb + scrolled tree view ──
    auto vbox = ase::adp::gtk::Box::vertical(0);

    vbox.append(m_breadcrumb.widget());

    auto scrolled = ase::adp::gtk::ScrolledWindow::create();
    scrolled.set_vexpand(true);
    scrolled.set_hexpand(true);
    scrolled.set_child(m_tree_view.list_view());
    vbox.append(scrolled);

    m_window.set_child(vbox);

    // ── Search bar handlers (delegate into this->handle_*) ──
    m_search_bar.entry().on_search_changed([this](const std::string& text) {
        handle_filter_changed(text);
    });
    m_search_bar.entry().on_stop_search([this]() {
        m_search_bar.toggle(false);
        // Restore the "ASE Explorer" title label that handle_search_toggle
        // hides when search opens. on_stop_search fires when the user
        // presses Escape inside the entry — without this restore the
        // header would stay empty until the search button is clicked again.
        if (m_title_label_native) {
            gtk_widget_set_visible(m_title_label_native, TRUE);
        }
    });

    // ── Breadcrumb segment click → navigate in place ──
    // Within the current root we expand ancestors and select the target
    // row so the tree stays loaded and only the clicked sub-path becomes
    // visible. Clicks on segments OUTSIDE the current root fall back to
    // load_root so the user can still jump upwards (e.g. via the
    // BREADCRUMB_BASE anchor levels).
    m_breadcrumb.on_segment_clicked([this](const std::string& path) {
        if (path == m_root_path) return;  // clicking current root is a no-op

        const bool inside_root =
            path.size() > m_root_path.size() &&
            path.compare(0, m_root_path.size(), m_root_path) == 0 &&
            path[m_root_path.size()] == '/';

        if (inside_root) {
            m_tree_view.navigate_to(path);
        } else {
            load_root(path);
        }
    });

    // ── File activation → consult FileAssociations and launch (no fallback) ──
    m_tree_view.on_file_activated([this](const std::string& path) {
        handle_file_activated(path);
    });

    // ── Mapping indicator dot: tree row factory consults this predicate ──
    m_tree_view.set_extension_mapping_check([this](const std::string& ext) {
        return !m_file_associations.lookup(ext).empty();
    });

    // ── Apply persisted breadcrumb max segments setting ──
    m_breadcrumb.set_max_segments(m_settings.breadcrumb_max_segments());

    // ── Tree selection change → breadcrumb update ──
    // Shows the absolute directory path of the currently-selected row as
    // clickable segments. Files never appear as trailing segments: when a
    // file is selected the breadcrumb shows the path to its containing
    // directory instead. Empty selection falls back to the current root so
    // the bar never becomes blank.
    m_tree_view.on_selection_changed([this](const std::string& selected) {
        if (selected.empty()) {
            m_breadcrumb.update(m_root_path);
            return;
        }
        const std::string dir = ase::utils::fs::is_directory(selected)
            ? selected
            : ase::utils::fs::parent_of(selected);
        m_breadcrumb.update(dir.empty() ? m_root_path : dir);
    });

    // ── Single-click gesture: toggle folders anywhere on the row ──
    // GtkGestureClick with n_press is unreliable for double-click on a
    // GtkListView because the listview's internal click handling consumes
    // the second event before our gesture sees n_press=2. We therefore
    // use the gesture ONLY for the custom n_press=1 folder-toggle and
    // delegate true row activation (= n_press=2 / Enter) to the canonical
    // GtkListView::activate signal below.
    auto click = ase::adp::gtk::ClickGesture::create();
    click.set_button(ase::adp::gtk::MouseButton::Primary);
    click.on_released([this](int n_press, double, double) {
        if (n_press == 1) m_tree_view.toggle_selected_folder();
    });
    m_tree_view.list_view().add_controller(click);

    // ── Canonical row activation (double-click + Enter) ──
    // GtkListView emits "activate" on its own — drop straight to the
    // C signal so the wrapper layer doesn't have to grow an `on_activate`.
    g_signal_connect(
        G_OBJECT(m_tree_view.list_view().native()->gobj()),
        "activate",
        G_CALLBACK(+[](GtkListView*, guint /*position*/, gpointer user_data) {
            auto* self = static_cast<ExplorerWindow*>(user_data);
            if (self) self->handle_activate_selection();
        }),
        this);

    // ── Context menu ──
    m_context_menu.on_open         ([this]() { handle_activate_selection(); });
    m_context_menu.on_open_with    ([this]() { handle_right_click_open_with(); });
    m_context_menu.on_copy_path    ([this]() { handle_copy_path(false); });
    m_context_menu.on_copy_rel_path([this]() { handle_copy_path(true); });
    m_context_menu.on_open_terminal([this]() { handle_right_click_open_terminal(); });
    m_context_menu.on_reveal       ([this]() { handle_right_click_reveal(); });
    m_context_menu.on_delete       ([this]() { handle_right_click_delete(); });
    m_context_menu.build(m_tree_view.list_view());

    // ── Drag source (drag selected paths into external apps) ──
    //
    // Multi-selection aware: every currently-selected row contributes one
    // GFile to the GdkFileList, and the text/plain fallback is a single
    // space-separated string of all absolute paths. File managers drop
    // every selected item as separate files; terminals and chat apps get
    // one text payload that can be pasted as a single shell argv line.
    //
    // GTK4-native file DnD via GdkFileList avoids text/uri-list CRLF
    // control-code false positives in terminal sanitiser checks.
    auto drag_source = ase::adp::gtk::DragSource::create();
    drag_source.set_actions(ase::adp::gtk::DragAction::Copy);
    drag_source.native()->signal_prepare().connect(
        [this](double, double) -> Glib::RefPtr<Gdk::ContentProvider> {
            const std::vector<std::string> paths = m_tree_view.selected_paths();
            if (paths.empty()) return {};

            // Build the GdkFileList: every selected absolute path becomes
            // a GFile in the list. Prepend in reverse order so the final
            // GSList order matches the selection order.
            std::vector<Glib::RefPtr<Gio::File>> file_refs;  // keep refs alive
            file_refs.reserve(paths.size());
            GSList* slist = nullptr;
            for (auto it = paths.rbegin(); it != paths.rend(); ++it) {
                auto gfile = Gio::File::create_for_path(*it);
                file_refs.push_back(gfile);
                slist = g_slist_prepend(slist, gfile->gobj());
            }
            GdkFileList* file_list = gdk_file_list_new_from_list(slist);
            g_slist_free(slist);

            GValue file_gvalue = G_VALUE_INIT;
            g_value_init(&file_gvalue, GDK_TYPE_FILE_LIST);
            g_value_take_boxed(&file_gvalue, file_list);
            GdkContentProvider* file_cp = gdk_content_provider_new_for_value(&file_gvalue);
            g_value_unset(&file_gvalue);
            auto file_provider = Glib::wrap(file_cp);

            // text/plain: all selected paths joined by a single space so a
            // terminal or chat app can paste them as one argv line. Each
            // path is passed through verbatim - callers that need shell
            // quoting can post-process.
            std::string joined;
            for (size_t i = 0; i < paths.size(); ++i) {
                if (i > 0) joined += ' ';
                joined += paths[i];
            }
            auto text_value = Glib::Value<Glib::ustring>();
            text_value.init(text_value.value_type());
            text_value.set(joined);
            auto text_provider = Gdk::ContentProvider::create(text_value);

            std::vector<Glib::RefPtr<Gdk::ContentProvider>> providers;
            providers.push_back(file_provider);
            providers.push_back(text_provider);
            return Gdk::ContentProvider::create(providers);
        }, false);
    m_tree_view.list_view().add_controller(drag_source);

    // ── Keyboard shortcuts ──
    m_shortcuts.on_refresh          ([this]() { refresh(); });
    m_shortcuts.on_settings         ([this]() { settings_dialog::show(m_window, m_file_associations, m_settings, m_root_path,
                              [this]() {
                                  m_breadcrumb.set_max_segments(m_settings.breadcrumb_max_segments());
                                  // If the user changed the default root, jump to it now;
                                  // otherwise just refresh so newly-mapped extensions show.
                                  if (m_settings.default_root() != m_root_path) {
                                      load_root(m_settings.default_root());
                                  } else {
                                      refresh();
                                  }
                              }); });
    m_shortcuts.on_toggle_search    ([this]() { handle_search_toggle(); });
    m_shortcuts.on_copy_absolute    ([this]() { handle_copy_path(false); });
    m_shortcuts.on_copy_relative    ([this]() { handle_copy_path(true); });
    m_shortcuts.on_activate_selection([this]() { handle_activate_selection(); });
    m_shortcuts.on_escape_in_search ([this]() { handle_escape_close_search(); });
    m_shortcuts.set_search_open_predicate([this]() -> bool { return m_search_bar.is_visible(); });
    m_shortcuts.build(m_window);

    // ── File watcher ──
    m_file_watcher.on_changed([this]() { refresh(); });

    // ── Escape on the main window: close the application ──
    // The keyboard_shortcuts module already routes Escape into the search
    // bar when it's open. When search is closed, Escape falls through to
    // here and closes the explorer (cascading window close convention).
    {
        GtkEventController* esc = gtk_event_controller_key_new();
        g_signal_connect(esc, "key-pressed",
            G_CALLBACK(+[](GtkEventControllerKey*, guint keyval, guint,
                           GdkModifierType, gpointer w) -> gboolean {
                if (keyval == GDK_KEY_Escape) {
                    gtk_window_close(GTK_WINDOW(w));
                    return TRUE;
                }
                return FALSE;
            }), m_window.native()->gobj());
        gtk_widget_add_controller(GTK_WIDGET(m_window.native()->gobj()), esc);
    }

    // ── Type-ahead search: any printable keystroke opens the search bar ──
    // Capture phase so this handler runs BEFORE the tree view's own key
    // controllers consume the keystroke. Returns TRUE only when we actually
    // hijack the event, so unrelated keys (arrows, Tab, F5, shortcuts …)
    // continue to propagate normally.
    {
        GtkEventController* type_ahead = gtk_event_controller_key_new();
        gtk_event_controller_set_propagation_phase(type_ahead, GTK_PHASE_CAPTURE);
        g_signal_connect(type_ahead, "key-pressed",
            G_CALLBACK(+[](GtkEventControllerKey*, guint keyval, guint,
                           GdkModifierType state, gpointer self) -> gboolean {
                auto* w = static_cast<ExplorerWindow*>(self);
                return w->handle_type_ahead(keyval, static_cast<unsigned>(state)) ? TRUE : FALSE;
            }), this);
        gtk_widget_add_controller(GTK_WIDGET(m_window.native()->gobj()), type_ahead);
    }
}

void ExplorerWindow::load_root(const std::string& path) {
    // Breadcrumb segments may point at files (the user clicked a file in
    // the tree; the breadcrumb shows the full selection path down to that
    // filename). Jumping to a file as a new root makes no sense - resolve
    // file paths to their containing directory instead.
    const std::string resolved = ase::utils::fs::is_directory(path)
        ? path
        : ase::utils::fs::parent_of(path);
    if (resolved.empty()) return;

    m_root_path = resolved;
    m_tree_view.populate(resolved);
    // Anchor the breadcrumb at the parent of the project root so the first
    // visible segment is always the project's basename.
    m_breadcrumb.set_base(ase::utils::fs::parent_of(resolved));
    m_breadcrumb.update(resolved);
    m_file_watcher.start(resolved);
    m_window.set_title("ASE Explorer \u2014 " + ase::utils::fs::filename_of(resolved));
}

void ExplorerWindow::load_default_root() {
    load_root(m_settings.default_root());
}

void ExplorerWindow::refresh() {
    if (m_root_path.empty()) return;
    m_tree_view.populate(m_root_path);
}

void ExplorerWindow::present() {
    m_window.present();
}

// ── Slot handlers ───────────────────────────────────────────────────

void ExplorerWindow::handle_activate_selection() {
    m_tree_view.activate_selection();
}

void ExplorerWindow::handle_right_click_open_with() {
    const std::string path = m_tree_view.selected_path();
    if (path.empty()) return;
    open_with_dialog::show(m_window, path, m_file_associations);
}

void ExplorerWindow::handle_file_activated(const std::string& path) {
    if (path.empty()) return;
    // Strict explicit-mapping policy: extract the lowercase extension and
    // look it up. If no mapping is configured, do nothing — there is no
    // implicit fallback to the OS-default app, by user policy.
    //
    // CRITICAL: parse the dot from the BASENAME, not the full path.
    // /a/foo.bar/baz would otherwise pick up ".bar/baz" as the extension.
    const std::string filename = ase::utils::fs::filename_of(path);
    const auto dot = filename.rfind('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 >= filename.size()) return;
    const std::string ext = filename.substr(dot + 1);
    const std::string desktop_id = m_file_associations.lookup(ext);
    if (desktop_id.empty()) return;
    app_catalog::launch(desktop_id, path);
}

void ExplorerWindow::handle_right_click_open_terminal() {
    auto info = m_tree_view.selected_file_info();
    if (!info.native()) return;
    auto full = info.get_full_path();
    if (full.empty()) return;
    auto dir = info.is_directory() ? full : ase::utils::fs::parent_of(full);
    ase::adp::gtk::spawn_command_async("foot --working-directory=" + dir);
}

void ExplorerWindow::handle_right_click_reveal() {
    auto info = m_tree_view.selected_file_info();
    if (!info.native()) return;
    auto full = info.get_full_path();
    if (full.empty()) return;
    auto dir = info.is_directory() ? full : ase::utils::fs::parent_of(full);
    auto file = ase::adp::gtk::File::create_for_path(dir);
    auto launcher = ase::adp::gtk::FileLauncher::create(file);
    launcher.launch(m_tree_view.list_view());
}

// File/folder deletion guarded by a modal confirmation. The alert dialog is
// the GTK4 native API (AdwMessageDialog is Adwaita-only and the explorer
// already uses raw gtk_* for its Open-With dialog). The continuation owns the
// state via unique_ptr so cancellation, confirmation, and error all release
// it exactly once.
void ExplorerWindow::handle_right_click_delete() {
    auto info = m_tree_view.selected_file_info();
    if (!info.native()) return;
    auto full = info.get_full_path();
    if (full.empty()) return;
    // Refuse to delete the current root — users should close / change root
    // instead, and blowing away the whole explorer root from a context
    // click is almost certainly an accident.
    if (full == m_root_path) return;

    const std::string name  = ase::utils::fs::filename_of(full);
    const bool        isdir = info.is_directory();
    const std::string question = isdir
        ? ("Delete folder \"" + name + "\" and all of its contents?")
        : ("Delete file \"" + name + "\"?");

    GtkAlertDialog* dialog = gtk_alert_dialog_new("%s", question.c_str());
    gtk_alert_dialog_set_detail(dialog, "This cannot be undone.");
    const char* buttons[] = { "Cancel", "Delete", nullptr };
    gtk_alert_dialog_set_buttons(dialog, buttons);
    gtk_alert_dialog_set_cancel_button(dialog, 0);
    gtk_alert_dialog_set_default_button(dialog, 0);

    struct CallbackState {
        ExplorerWindow* self;
        std::string     path;
    };
    auto state = std::make_unique<CallbackState>(CallbackState{ this, std::move(full) });

    GtkWindow* parent = GTK_WINDOW(m_window.native()->gobj());
    gtk_alert_dialog_choose(
        dialog, parent, nullptr,
        [](GObject* src, GAsyncResult* res, gpointer user_data) {
            std::unique_ptr<CallbackState> s(static_cast<CallbackState*>(user_data));
            GError* err = nullptr;
            int choice = gtk_alert_dialog_choose_finish(GTK_ALERT_DIALOG(src), res, &err);
            if (err) { g_error_free(err); return; }
            if (choice == 1) {
                ase::utils::fs::remove(s->path);
                s->self->refresh();
            }
        },
        state.release());
    g_object_unref(dialog);
}

void ExplorerWindow::handle_copy_path(bool relative) {
    auto path = m_tree_view.selected_path();
    clipboard_ops::copy_path(m_tree_view.list_view(), path, m_root_path, relative);
}

void ExplorerWindow::handle_search_toggle() {
    const bool show = !m_search_bar.is_visible();
    m_search_bar.toggle(show);
    // Hide the "ASE Explorer" title label when search is active so the
    // entry fills the header bar end-to-end (between pack_start = empty
    // and pack_end = button cluster). Restore it when search closes.
    if (m_title_label_native) {
        gtk_widget_set_visible(m_title_label_native, !show);
    }
}

void ExplorerWindow::handle_escape_close_search() {
    m_search_bar.toggle(false);
    // Restore the title label that handle_search_toggle hid.
    if (m_title_label_native) {
        gtk_widget_set_visible(m_title_label_native, TRUE);
    }
}

void ExplorerWindow::handle_filter_changed(const std::string& text) {
    // Forward the filter string to the tree view, which re-populates with
    // a name-substring match applied to files. Directories always remain
    // visible so the user can keep navigating into them.
    m_tree_view.set_filter(text);
}

bool ExplorerWindow::handle_type_ahead(unsigned keyval, unsigned state) {
    // Already in search mode → let the entry handle the key normally.
    if (m_search_bar.is_visible()) return false;

    // Skip when any modifier is held so Ctrl+F, Ctrl+C, F5, Alt+… still
    // route to their own handlers / shortcuts.
    constexpr unsigned MODIFIERS =
        static_cast<unsigned>(GDK_CONTROL_MASK) |
        static_cast<unsigned>(GDK_ALT_MASK) |
        static_cast<unsigned>(GDK_SUPER_MASK) |
        static_cast<unsigned>(GDK_META_MASK);
    if (state & MODIFIERS) return false;

    // Convert the GDK keyval to a Unicode codepoint. Non-printable keys
    // (arrows, Enter, Tab, F-keys, Escape …) return 0 or a control char.
    const gunichar uc = gdk_keyval_to_unicode(keyval);
    if (uc == 0 || g_unichar_iscntrl(uc)) return false;

    // Open the search bar (this also hides the title label via
    // handle_search_toggle's wiring).
    handle_search_toggle();

    // Seed the entry with the typed character.
    char utf8[8] = {0};
    const int len = g_unichar_to_utf8(uc, utf8);
    utf8[len] = '\0';
    m_search_bar.entry().set_text(utf8);

    // GtkEditable::set_text leaves the cursor at its previous position
    // (0 for a freshly-shown entry), so subsequent keystrokes would
    // insert BEFORE the seeded character (typing c, p, p would show
    // "ppc" instead of "cpp"). Move the cursor to the end explicitly.
    gtk_editable_set_position(
        GTK_EDITABLE(m_search_bar.entry().native()->gobj()), -1);

    return true;
}

}  // namespace ase::explorer

/**
 * ASE EXPLORER NATIVE - ASSOCIATIONS PAGE
 *
 * @file        settings_assoc_page.cpp
 * @brief       Assembles the Associations page of the preferences dialog and wires its callbacks
 * @description A three-zone cockpit over the FileAssociations store. The left pane lists
 *              the file extensions extension_scan found under the current root, the right
 *              pane every installed XDG application from app_catalog, the bottom strip the
 *              active mappings as removable chips. Click an extension on the left and it
 *              becomes the active extension; click an application on the right and the
 *              mapping is created for it, saved, and drawn anew by settings_assoc_view.cpp.
 *
 *              This file holds the page assembly, the two search filters and the two
 *              selection callbacks; what a state looks like on screen stands in the view.
 *
 * @module      ase-client-explorer
 * @layer       5 (Clients)
 * @category    render/ui/widget
 * @usecase     usecase:101130 - map an extension
 * @created     2026-10-19
 * @modified    2026-10-19
 * @version     00.00.01.00001 [seed]
 *
 * CLIENT NATIVE COMPLIANCE
 *
 * [ ] English only - identifiers, comments, messages
 * [ ] META fields complete, @layer 5 (Section 0)
 * [ ] Headings are bare capital lines, cuts are empty lines (Section 0.1)
 * [ ] Form defined in INST_ASE_LINT.md Section 23
 */

#include <explorer/settings_page.hpp>

#include <explorer/file_associations.hpp>
#include <explorer/settings_assoc_view.hpp>

#include <ase/adp/adw/cockpit.hpp>

#include "design_tokens.hpp"

#include <gtk/gtk.h>

#include <cctype>
#include <memory>
#include <string>

namespace ase::explorer::settings_page {

namespace {

namespace t = ase::theme;

using ase::adp::adw::cockpit::make_section_strip;

// LAYOUT
//
//   ┌─ FILE EXTENSIONS IN PROJECT ─┬─ INSTALLED APPLICATIONS ──────────┐
//   │ .cpp  47  → Sublime Text     │ [search…]                         │
//   │ .hpp  32                     │ Sublime Text   subl.desktop       │
//   │ .md   12  → Typora           │ Kitty          kitty.desktop      │
//   │ ...                          │ ...                               │
//   ├─ ACTIVE MAPPINGS ────────────┴───────────────────────────────────┤
//   │ [.cpp → Sublime Text ×]  [.md → Typora ×]  [.json → VS Code ×]   │
//   └──────────────────────────────────────────────────────────────────┘

// Case-insensitive substring search reused by both filter callbacks.
bool contains_ci(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    if (haystack.size() < needle.size()) return false;
    for (size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
        bool ok = true;
        for (size_t j = 0; j < needle.size(); ++j) {
            char a = std::tolower(static_cast<unsigned char>(haystack[i + j]));
            char b = std::tolower(static_cast<unsigned char>(needle[j]));
            if (a != b) { ok = false; break; }
        }
        if (ok) return true;
    }
    return false;
}

gboolean apps_filter_cb(GtkListBoxRow* row, gpointer user_data) {
    auto* state = static_cast<CockpitState*>(user_data);
    if (state->app_search_text.empty()) return TRUE;
    auto* entry_ptr = static_cast<AppEntry*>(g_object_get_data(G_OBJECT(row), "ase-app-entry"));
    if (!entry_ptr) return FALSE;
    return (contains_ci(entry_ptr->name,       state->app_search_text)
         || contains_ci(entry_ptr->exec,       state->app_search_text)
         || contains_ci(entry_ptr->desktop_id, state->app_search_text)) ? TRUE : FALSE;
}

gboolean exts_filter_cb(GtkListBoxRow* row, gpointer user_data) {
    auto* state = static_cast<CockpitState*>(user_data);
    if (state->ext_search_text.empty()) return TRUE;
    const char* ext = static_cast<const char*>(g_object_get_data(G_OBJECT(row), "ase-ext"));
    if (!ext) return FALSE;
    return contains_ci(std::string(ext), state->ext_search_text) ? TRUE : FALSE;
}

void on_apps_search_changed_cb(GtkSearchEntry* entry, gpointer user_data) {
    auto* state = static_cast<CockpitState*>(user_data);
    const char* text = gtk_editable_get_text(GTK_EDITABLE(entry));
    state->app_search_text = text ? text : "";
    gtk_list_box_invalidate_filter(GTK_LIST_BOX(state->apps_listbox));
}

void on_ext_search_changed_cb(GtkSearchEntry* entry, gpointer user_data) {
    auto* state = static_cast<CockpitState*>(user_data);
    const char* text = gtk_editable_get_text(GTK_EDITABLE(entry));
    state->ext_search_text = text ? text : "";
    gtk_list_box_invalidate_filter(GTK_LIST_BOX(state->extensions_listbox));
}

void on_extension_row_selected_cb(GtkListBox*, GtkListBoxRow* row, gpointer user_data) {
    auto* state = static_cast<CockpitState*>(user_data);
    if (!row) {
        state->selected_extension.clear();
        update_hint(state);
        return;
    }
    const char* ext = static_cast<const char*>(g_object_get_data(G_OBJECT(row), "ase-ext"));
    state->selected_extension = ext ? ext : "";
    update_hint(state);
}

void on_app_row_selected_cb(GtkListBox*, GtkListBoxRow* row, gpointer user_data) {
    auto* state = static_cast<CockpitState*>(user_data);
    if (!row || !state->store) return;
    if (state->suppress_app_select) return;

    if (state->selected_extension.empty()) {
        update_hint(state);
        return;
    }

    auto* entry_ptr = static_cast<AppEntry*>(g_object_get_data(G_OBJECT(row), "ase-app-entry"));
    if (!entry_ptr) return;

    state->store->set(state->selected_extension, entry_ptr->desktop_id);
    state->store->save();

    rebuild_extensions_listbox(state);
    rebuild_chips_strip(state);
    update_hint(state);

    // Drop the right-pane selection so the next ext→click cycle starts
    // with a clean slate. Suppress the recursive on_app_row_selected fire.
    state->suppress_app_select = true;
    gtk_list_box_unselect_all(GTK_LIST_BOX(state->apps_listbox));
    state->suppress_app_select = false;
}

}  // namespace

GtkWidget* build_associations_page(FileAssociations& associations,
                                   const std::string& root_path)
{
    auto state = std::make_shared<CockpitState>();
    state->store    = &associations;
    state->all_apps = app_catalog::all();
    state->scanned  = extension_scan::scan(root_path);

    // Outer page: vertical so the chip strip can sit at the bottom across
    // both panes. Top half is a horizontal split (extensions | apps).
    GtkWidget* page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(page, "ase-cfg-window");
    gtk_widget_set_hexpand(page, TRUE);
    gtk_widget_set_vexpand(page, TRUE);

    GtkWidget* top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(top, TRUE);
    gtk_widget_set_vexpand(top, TRUE);

    // Helper: build a search-entry row identical on both panes so the layout
    // is symmetric. The placeholder + signal handler differ per pane.
    auto make_search_row = [](GtkWidget* entry) {
        GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_set_margin_start (box, t::space_md);
        gtk_widget_set_margin_end   (box, t::space_md);
        gtk_widget_set_margin_top   (box, t::space_sm);
        gtk_widget_set_margin_bottom(box, t::space_sm);
        gtk_widget_set_hexpand(entry, TRUE);
        gtk_box_append(GTK_BOX(box), entry);
        return box;
    };

    // LEFT PANE - scanned extensions
    GtkWidget* left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(left, TRUE);
    gtk_widget_set_vexpand(left, TRUE);
    gtk_widget_add_css_class(left, "ase-cfg-pane");

    gtk_box_append(GTK_BOX(left), make_section_strip("FILE EXTENSIONS IN PROJECT"));

    GtkWidget* ext_search_entry = gtk_search_entry_new();
    gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(ext_search_entry),
        "Filter extensions…");
    state->ext_search_entry = ext_search_entry;
    g_signal_connect(ext_search_entry, "search-changed",
        G_CALLBACK(on_ext_search_changed_cb), state.get());
    gtk_box_append(GTK_BOX(left), make_search_row(ext_search_entry));

    GtkWidget* ext_scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(ext_scroll, TRUE);
    gtk_widget_set_hexpand(ext_scroll, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ext_scroll),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget* ext_listbox = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(ext_listbox), GTK_SELECTION_BROWSE);
    gtk_widget_add_css_class(ext_listbox, "ase-cfg-window");
    state->extensions_listbox = ext_listbox;
    gtk_list_box_set_filter_func(GTK_LIST_BOX(ext_listbox),
        exts_filter_cb, state.get(), nullptr);
    g_signal_connect(ext_listbox, "row-selected",
        G_CALLBACK(on_extension_row_selected_cb), state.get());
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(ext_scroll), ext_listbox);
    gtk_box_append(GTK_BOX(left), ext_scroll);

    rebuild_extensions_listbox(state.get());

    // DIVIDER
    GtkWidget* divider = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(divider, "ase-cfg-pane-divider");
    gtk_widget_set_size_request(divider, 1, -1);

    // RIGHT PANE - application picker, built like the left pane
    GtkWidget* right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(right, TRUE);
    gtk_widget_set_vexpand(right, TRUE);
    gtk_widget_add_css_class(right, "ase-cfg-pane");

    gtk_box_append(GTK_BOX(right), make_section_strip("INSTALLED APPLICATIONS"));

    GtkWidget* app_search_entry = gtk_search_entry_new();
    gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(app_search_entry),
        "Filter applications…");
    state->app_search_entry = app_search_entry;
    g_signal_connect(app_search_entry, "search-changed",
        G_CALLBACK(on_apps_search_changed_cb), state.get());
    gtk_box_append(GTK_BOX(right), make_search_row(app_search_entry));

    GtkWidget* apps_scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(apps_scroll, TRUE);
    gtk_widget_set_hexpand(apps_scroll, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(apps_scroll),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget* apps_listbox = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(apps_listbox), GTK_SELECTION_BROWSE);
    gtk_widget_add_css_class(apps_listbox, "ase-cfg-window");
    state->apps_listbox = apps_listbox;
    gtk_list_box_set_filter_func(GTK_LIST_BOX(apps_listbox),
        apps_filter_cb, state.get(), nullptr);
    g_signal_connect(apps_listbox, "row-selected",
        G_CALLBACK(on_app_row_selected_cb), state.get());

    for (auto& app : state->all_apps) {
        GtkWidget* row = build_app_picker_row(app);
        g_object_set_data(G_OBJECT(row), "ase-app-entry", &app);
        gtk_list_box_append(GTK_LIST_BOX(apps_listbox), row);
    }

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(apps_scroll), apps_listbox);
    gtk_box_append(GTK_BOX(right), apps_scroll);

    gtk_box_append(GTK_BOX(top), left);
    gtk_box_append(GTK_BOX(top), divider);
    gtk_box_append(GTK_BOX(top), right);
    gtk_box_append(GTK_BOX(page), top);

    // BOTTOM - the ACTIVE MAPPINGS chip strip across the full width
    gtk_box_append(GTK_BOX(page), make_section_strip("ACTIVE MAPPINGS"));

    GtkWidget* hint = gtk_label_new("");
    gtk_widget_add_css_class(hint, "ase-cfg-hint");
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
    state->hint_label = hint;
    gtk_box_append(GTK_BOX(page), hint);

    GtkWidget* chips_scroll = gtk_scrolled_window_new();
    gtk_widget_set_hexpand(chips_scroll, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(chips_scroll),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
    gtk_widget_set_size_request(chips_scroll, -1, t::space_2xl + t::space_sm);  // ~56 px

    GtkWidget* chips_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, t::space_xs);
    gtk_widget_set_margin_start (chips_box, t::space_md);
    gtk_widget_set_margin_end   (chips_box, t::space_md);
    gtk_widget_set_margin_top   (chips_box, t::space_sm);
    gtk_widget_set_margin_bottom(chips_box, t::space_sm);
    state->chips_box = chips_box;

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(chips_scroll), chips_box);
    gtk_box_append(GTK_BOX(page), chips_scroll);

    rebuild_chips_strip(state.get());
    update_hint(state.get());

    // Pin the shared state to the page so all callbacks remain valid. The holder block
    // comes from GLib and is constructed in place; the destroy notify runs the destructor
    // by hand before the block goes back. The folder picker of ase-adp-gtk holds its
    // state the same way: the widget lifetime governs the teardown, not a C++ scope.
    using CockpitStateHolder = std::shared_ptr<CockpitState>;
    auto* state_holder =
        static_cast<CockpitStateHolder*>(g_malloc0(sizeof(CockpitStateHolder)));
    new (state_holder) CockpitStateHolder(state);
    g_object_set_data_full(
        G_OBJECT(page),
        "ase-cockpit-state",
        state_holder,
        +[](gpointer p) {
            auto* holder = static_cast<CockpitStateHolder*>(p);
            if (!holder) return;
            holder->~CockpitStateHolder();  // drops the last reference to CockpitState
            g_free(holder);
        });

    return page;
}

}  // namespace ase::explorer::settings_page

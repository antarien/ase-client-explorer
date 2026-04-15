/**
 * @file        settings_dialog.cpp
 * @brief       Implementation for settings_dialog.hpp
 * @description Cockpit-style settings dialog. Drops AdwPreferencesPage
 *              (which centers content at ~600px max-width) entirely in
 *              favour of dense, full-width raw GTK pages added directly
 *              to an AdwViewStack via the libadwaita C API.
 *
 *              Pages:
 *                Display      — file-display toggles in a flush 3-column row
 *                Behavior     — interaction toggles + terminal dropdown
 *                Associations — 2-pane horizontal split (left: current
 *                               mappings + add row; right: searchable
 *                               app picker over every installed XDG app)
 *
 *              Every spacing / font-size / padding value is sourced from
 *              ase::theme::* (design_tokens.hpp); every colour from
 *              ase::colors::* (colors.hpp). The visual look (uppercase
 *              tabs in MENU_RED, monospace, MENU_RED section strips)
 *              is provided by the .ase-cfg-* CSS classes in theme.cpp.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <explorer/settings_dialog.hpp>

#include <explorer/app_catalog.hpp>
#include <explorer/explorer_settings.hpp>
#include <explorer/extension_scan.hpp>
#include <explorer/file_associations.hpp>
#include <explorer/folder_picker.hpp>
#include <explorer/icons.hpp>

#include <ase/adp/adw/adw.hpp>

#include "colors.hpp"
#include "design_tokens.hpp"
#include "ui_icons.hpp"

#include <adwaita.h>
#include <gtk/gtk.h>

#include <cctype>
#include <memory>
#include <string>
#include <vector>

namespace ase::explorer::settings_dialog {

namespace {

namespace t = ase::theme;

// Cockpit window default — explorer-local layout decision, deliberately
// expressed in terms of the official spacing scale so any future global
// scale tweak proportionally adjusts the dialog as well.
constexpr int CFG_WIDTH  = t::space_2xl * 35 + t::space_md;   // 1696
constexpr int CFG_HEIGHT = t::space_2xl * 21 + t::space_xl;   // 1040

// ── Small builders ──────────────────────────────────────────────────

GtkWidget* make_section_strip(const char* title) {
    GtkWidget* label = gtk_label_new(title);
    gtk_widget_add_css_class(label, "ase-cfg-section-title");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_set_hexpand(label, TRUE);
    return label;
}

// A flush "cell" hosting one labelled control. Stacks an uppercase label
// above the actual control. Used by the Display + Behavior tabs.
GtkWidget* make_cell(const char* label_text, GtkWidget* control) {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, t::space_xs);
    gtk_widget_add_css_class(box, "ase-cfg-cell");
    gtk_widget_set_hexpand(box, TRUE);
    gtk_widget_set_vexpand(box, FALSE);

    GtkWidget* label = gtk_label_new(label_text);
    gtk_widget_add_css_class(label, "ase-cfg-cell-label");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_box_append(GTK_BOX(box), label);

    GtkWidget* value_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, t::space_sm);
    gtk_widget_add_css_class(value_row, "ase-cfg-cell-value");
    gtk_widget_set_hexpand(value_row, TRUE);
    gtk_box_append(GTK_BOX(value_row), control);
    gtk_widget_set_hexpand(control, TRUE);
    gtk_widget_set_halign(control, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), value_row);

    return box;
}

GtkWidget* make_toggle(bool initial) {
    GtkWidget* sw = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(sw), initial ? TRUE : FALSE);
    gtk_widget_set_valign(sw, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(sw, GTK_ALIGN_START);
    return sw;
}

GtkWidget* make_dropdown(const std::vector<const char*>& choices, unsigned selected) {
    std::vector<const char*> with_null(choices);
    with_null.push_back(nullptr);
    GtkWidget* dd = gtk_drop_down_new_from_strings(with_null.data());
    gtk_drop_down_set_selected(GTK_DROP_DOWN(dd), selected);
    gtk_widget_set_valign(dd, GTK_ALIGN_CENTER);
    return dd;
}

// A horizontal row of cells. Cells share the row evenly via hexpand.
GtkWidget* make_cell_row(std::initializer_list<GtkWidget*> cells) {
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(row, TRUE);
    for (GtkWidget* cell : cells) {
        gtk_box_append(GTK_BOX(row), cell);
    }
    return row;
}

// ── Display / Behavior pages ─────────────────────────────────────────

// SpinButton callback that writes the new value back into ExplorerSettings
// and persists immediately. The settings reference is captured by pointer in
// user_data via g_object_set_data.
void on_breadcrumb_max_changed_cb(GtkSpinButton* sb, gpointer user_data) {
    auto* settings = static_cast<ExplorerSettings*>(user_data);
    if (!settings) return;
    const int value = static_cast<int>(gtk_spin_button_get_value(sb));
    settings->set_breadcrumb_max_segments(value);
    settings->save();
}

// Entry callback for the default root path. Persists on every keystroke
// so the on_close hook in window.cpp can pick up the new value.
void on_default_root_changed_cb(GtkEditable* editable, gpointer user_data) {
    auto* settings = static_cast<ExplorerSettings*>(user_data);
    if (!settings) return;
    const char* text = gtk_editable_get_text(editable);
    if (!text) return;
    settings->set_default_root(text);
    settings->save();
}

// State owned by the "browse for root" button — the entry widget to write
// back into and the settings store to persist. Allocated once at dialog
// build time, attached to the button via g_object_set_data_full so it
// dies with the button.
struct RootBrowseState {
    GtkWidget*        entry    = nullptr;
    ExplorerSettings* settings = nullptr;
};

// Browse-button click handler. Routes to the in-process folder_picker
// instead of GtkFileDialog because the latter (a) requires an
// xdg-desktop-portal FileChooser backend that isn't available on a pure
// Hyprland session, and (b) when it falls back to its own internal
// GtkPathBar-based chooser it triggers a known "gtk_box_remove:
// GTK_IS_BOX (box) failed" cascade and never shows a picker.
//
// folder_picker is built from raw GTK4 widgets we control end-to-end.
void on_browse_root_clicked_cb(GtkButton* btn, gpointer user_data) {
    auto* bs = static_cast<RootBrowseState*>(user_data);
    if (!bs || !bs->entry) return;

    GtkRoot* root = gtk_widget_get_root(GTK_WIDGET(btn));
    GtkWindow* parent = (root && GTK_IS_WINDOW(root)) ? GTK_WINDOW(root) : nullptr;

    const char* current = gtk_editable_get_text(GTK_EDITABLE(bs->entry));
    const std::string start = current ? std::string(current) : std::string();

    folder_picker::show(parent, start,
        [bs](const std::string& chosen) {
            if (!bs || !bs->entry || !bs->settings) return;
            gtk_editable_set_text(GTK_EDITABLE(bs->entry), chosen.c_str());
            bs->settings->set_default_root(chosen);
            bs->settings->save();
        });
}

GtkWidget* build_display_page(ExplorerSettings& settings) {
    GtkWidget* page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(page, "ase-cfg-window");
    gtk_widget_set_hexpand(page, TRUE);
    gtk_widget_set_vexpand(page, TRUE);

    gtk_box_append(GTK_BOX(page), make_section_strip("FILE DISPLAY"));
    gtk_box_append(GTK_BOX(page), make_cell_row({
        make_cell("HIDDEN FILES",       make_toggle(false)),
        make_cell("GITIGNORED FILES",   make_toggle(false)),
        make_cell("COMPACT MODE",       make_toggle(false)),
    }));

    gtk_box_append(GTK_BOX(page), make_section_strip("ROOT DIRECTORY"));

    // The default-root cell is laid out by hand because make_cell() forces
    // its child to halign=START — we want the entry to grow into the full
    // available width so the user can read the entire path.
    GtkWidget* root_cell = gtk_box_new(GTK_ORIENTATION_VERTICAL, t::space_xs);
    gtk_widget_add_css_class(root_cell, "ase-cfg-cell");
    gtk_widget_set_hexpand(root_cell, TRUE);

    GtkWidget* root_label = gtk_label_new("DEFAULT ROOT PATH");
    gtk_widget_add_css_class(root_label, "ase-cfg-cell-label");
    gtk_label_set_xalign(GTK_LABEL(root_label), 0.0f);
    gtk_box_append(GTK_BOX(root_cell), root_label);

    GtkWidget* root_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, t::space_sm);
    gtk_widget_set_hexpand(root_row, TRUE);
    gtk_widget_set_halign(root_row, GTK_ALIGN_FILL);

    GtkWidget* root_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(root_entry), settings.default_root().c_str());
    gtk_widget_set_tooltip_text(root_entry,
        "Absolute path. Takes effect when this dialog closes.");
    gtk_widget_set_hexpand(root_entry, TRUE);
    gtk_widget_set_halign(root_entry, GTK_ALIGN_FILL);
    g_signal_connect(root_entry, "changed",
        G_CALLBACK(on_default_root_changed_cb), &settings);
    gtk_box_append(GTK_BOX(root_row), root_entry);

    GtkWidget* browse_btn = gtk_button_new_with_label("…");
    gtk_widget_set_tooltip_text(browse_btn, "Browse for a directory…");
    gtk_widget_set_valign(browse_btn, GTK_ALIGN_CENTER);

    auto* bs = g_new0(RootBrowseState, 1);
    bs->entry    = root_entry;
    bs->settings = &settings;
    g_object_set_data_full(G_OBJECT(browse_btn), "ase-browse-state", bs,
        +[](gpointer p) { g_free(p); });
    g_signal_connect(browse_btn, "clicked",
        G_CALLBACK(on_browse_root_clicked_cb), bs);
    gtk_box_append(GTK_BOX(root_row), browse_btn);

    gtk_box_append(GTK_BOX(root_cell), root_row);
    gtk_box_append(GTK_BOX(page), root_cell);

    gtk_box_append(GTK_BOX(page), make_section_strip("BREADCRUMB"));

    GtkWidget* breadcrumb_spin = gtk_spin_button_new_with_range(3.0, 20.0, 1.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(breadcrumb_spin),
        static_cast<double>(settings.breadcrumb_max_segments()));
    g_signal_connect(breadcrumb_spin, "value-changed",
        G_CALLBACK(on_breadcrumb_max_changed_cb), &settings);
    gtk_widget_set_valign(breadcrumb_spin, GTK_ALIGN_CENTER);

    gtk_box_append(GTK_BOX(page), make_cell_row({
        make_cell("MAX SEGMENTS", breadcrumb_spin),
    }));

    // Trailing filler so the section strips don't stretch to fill the
    // entire page height — the cockpit aesthetic wants strips packed at
    // the top with empty space below for future expansion.
    GtkWidget* spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(page), spacer);

    return page;
}

GtkWidget* build_behavior_page() {
    GtkWidget* page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(page, "ase-cfg-window");
    gtk_widget_set_hexpand(page, TRUE);
    gtk_widget_set_vexpand(page, TRUE);

    gtk_box_append(GTK_BOX(page), make_section_strip("INTERACTION"));
    gtk_box_append(GTK_BOX(page), make_cell_row({
        make_cell("SINGLE-CLICK OPEN",  make_toggle(false)),
        make_cell("LIVE FILE WATCHING", make_toggle(true)),
    }));

    gtk_box_append(GTK_BOX(page), make_section_strip("TERMINAL"));
    gtk_box_append(GTK_BOX(page), make_cell_row({
        make_cell("EMULATOR",
            make_dropdown({"foot", "kitty", "alacritty", "ghostty", "wezterm"}, 0)),
    }));

    GtkWidget* spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(page), spacer);

    return page;
}

// ── Associations page (3-zone cockpit) ───────────────────────────────
//
// Layout:
//   ┌─ FILE EXTENSIONS IN PROJECT ─┬─ INSTALLED APPLICATIONS ──────────┐
//   │ .cpp  47  → Sublime Text     │ [search…]                         │
//   │ .hpp  32                     │ Sublime Text   subl.desktop       │
//   │ .md   12  → Typora           │ Kitty          kitty.desktop      │
//   │ ...                          │ ...                               │
//   ├─ ACTIVE MAPPINGS ────────────┴───────────────────────────────────┤
//   │ [.cpp → Sublime Text ×]  [.md → Typora ×]  [.json → VS Code ×]   │
//   └──────────────────────────────────────────────────────────────────┘
//
// Workflow: click extension on left → it's the "active extension". Click an
// app on right → mapping is created instantly for the active extension,
// persisted, and visible in the bottom chip strip + inline on the left row.

struct CockpitState {
    FileAssociations*                          store = nullptr;
    std::vector<AppEntry>                      all_apps;
    std::vector<extension_scan::ExtensionCount> scanned;
    GtkWidget*                                 extensions_listbox = nullptr;
    GtkWidget*                                 apps_listbox       = nullptr;
    GtkWidget*                                 ext_search_entry   = nullptr;
    GtkWidget*                                 app_search_entry   = nullptr;
    GtkWidget*                                 chips_box          = nullptr;
    GtkWidget*                                 hint_label         = nullptr;
    std::string                                ext_search_text;
    std::string                                app_search_text;
    std::string                                selected_extension;
    bool                                       suppress_app_select = false;
};

void rebuild_extensions_listbox(CockpitState* state);
void rebuild_chips_strip(CockpitState* state);
void update_hint(CockpitState* state);

GtkWidget* build_extension_row(const extension_scan::ExtensionCount& ec,
                               const std::string& mapped_app_name)
{
    GtkWidget* row = gtk_list_box_row_new();
    gtk_widget_add_css_class(row, "ase-cfg-ext-row");

    GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, t::space_md);

    GtkWidget* ext_label = gtk_label_new(("." + ec.extension).c_str());
    gtk_widget_add_css_class(ext_label, "ase-cfg-ext-name");
    gtk_widget_set_size_request(ext_label, t::space_2xl + t::space_lg, -1);  // ~72 px
    gtk_label_set_xalign(GTK_LABEL(ext_label), 0.0f);
    gtk_box_append(GTK_BOX(hbox), ext_label);

    GtkWidget* count_label = gtk_label_new((std::to_string(ec.count) + " files").c_str());
    gtk_widget_add_css_class(count_label, "ase-cfg-ext-count");
    gtk_widget_set_size_request(count_label, t::space_2xl + t::space_md, -1);  // ~64 px
    gtk_label_set_xalign(GTK_LABEL(count_label), 0.0f);
    gtk_box_append(GTK_BOX(hbox), count_label);

    GtkWidget* mapped_label = gtk_label_new(
        mapped_app_name.empty() ? "" : ("→ " + mapped_app_name).c_str());
    gtk_widget_add_css_class(mapped_label, "ase-cfg-ext-mapped");
    gtk_widget_set_hexpand(mapped_label, TRUE);
    gtk_label_set_xalign(GTK_LABEL(mapped_label), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(mapped_label), PANGO_ELLIPSIZE_END);
    gtk_box_append(GTK_BOX(hbox), mapped_label);

    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), hbox);
    return row;
}

void rebuild_extensions_listbox(CockpitState* state) {
    if (!state || !state->extensions_listbox) return;

    GtkWidget* child = gtk_widget_get_first_child(state->extensions_listbox);
    while (child) {
        GtkWidget* next = gtk_widget_get_next_sibling(child);
        gtk_list_box_remove(GTK_LIST_BOX(state->extensions_listbox), child);
        child = next;
    }

    if (state->scanned.empty()) {
        GtkWidget* row = gtk_list_box_row_new();
        gtk_widget_add_css_class(row, "ase-cfg-ext-row");
        gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(row), FALSE);
        GtkWidget* msg = gtk_label_new("No files in current root — open a directory first.");
        gtk_widget_add_css_class(msg, "ase-cfg-ext-count");
        gtk_label_set_xalign(GTK_LABEL(msg), 0.0f);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), msg);
        gtk_list_box_append(GTK_LIST_BOX(state->extensions_listbox), row);
        return;
    }

    GtkListBoxRow* row_to_select = nullptr;
    for (const auto& ec : state->scanned) {
        const std::string desktop_id = state->store->lookup(ec.extension);
        std::string app_name;
        if (!desktop_id.empty()) {
            const AppEntry app = app_catalog::find_by_id(desktop_id);
            app_name = app.name.empty() ? desktop_id : app.name;
        }
        GtkWidget* row = build_extension_row(ec, app_name);

        // Stash the extension on the row so the selection handler can read
        // it back without needing a side index.
        char* ext_dup = g_strdup(ec.extension.c_str());
        g_object_set_data_full(G_OBJECT(row), "ase-ext", ext_dup, g_free);

        gtk_list_box_append(GTK_LIST_BOX(state->extensions_listbox), row);

        if (ec.extension == state->selected_extension) {
            row_to_select = GTK_LIST_BOX_ROW(row);
        }
    }

    if (row_to_select) {
        gtk_list_box_select_row(GTK_LIST_BOX(state->extensions_listbox), row_to_select);
    }
}

GtkWidget* build_chip(const std::string& extension,
                      const std::string& desktop_id,
                      CockpitState* state)
{
    GtkWidget* chip = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, t::space_xs);
    gtk_widget_add_css_class(chip, "ase-cfg-chip");

    GtkWidget* ext_label = gtk_label_new(("." + extension).c_str());
    gtk_widget_add_css_class(ext_label, "ase-cfg-chip-ext");
    gtk_box_append(GTK_BOX(chip), ext_label);

    GtkWidget* arrow = gtk_label_new("→");
    gtk_widget_add_css_class(arrow, "ase-cfg-chip-arrow");
    gtk_box_append(GTK_BOX(chip), arrow);

    AppEntry app = app_catalog::find_by_id(desktop_id);
    GtkWidget* app_label = gtk_label_new(
        app.name.empty() ? desktop_id.c_str() : app.name.c_str());
    gtk_widget_add_css_class(app_label, "ase-cfg-chip-app");
    gtk_box_append(GTK_BOX(chip), app_label);

    GtkWidget* remove_btn = ase::explorer::icons::make_glyph_button(
        ase::ui_icons::ICON_TIMES,
        ase::colors::TEXT_LABEL & 0xFFFFFF,
        ase::explorer::icons::ICON_FONT_SIZE - t::space_xs,
        "Remove this association");
    char* ext_dup = g_strdup(extension.c_str());
    g_object_set_data_full(G_OBJECT(remove_btn), "ase-ext", ext_dup, g_free);
    g_signal_connect(remove_btn, "clicked",
        G_CALLBACK(+[](GtkButton* btn, gpointer user_data) {
            auto* st = static_cast<CockpitState*>(user_data);
            const char* ext = static_cast<const char*>(g_object_get_data(G_OBJECT(btn), "ase-ext"));
            if (!ext || !st->store) return;
            st->store->remove(ext);
            st->store->save();
            // CRITICAL: rebuild_chips_strip would unparent THIS chip while
            // its child × button's click handler is still on the call
            // stack. Defer to the next idle so GTK finishes dispatching
            // this event first; then the chip can be torn down cleanly.
            g_idle_add_once(+[](gpointer data) {
                auto* s = static_cast<CockpitState*>(data);
                rebuild_extensions_listbox(s);
                rebuild_chips_strip(s);
                update_hint(s);
            }, st);
        }), state);
    gtk_box_append(GTK_BOX(chip), remove_btn);

    return chip;
}

void rebuild_chips_strip(CockpitState* state) {
    if (!state || !state->chips_box) return;

    GtkWidget* child = gtk_widget_get_first_child(state->chips_box);
    while (child) {
        GtkWidget* next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(GTK_BOX(state->chips_box), child);
        child = next;
    }

    const auto all = state->store->all();
    if (all.empty()) {
        GtkWidget* empty = gtk_label_new("No mappings yet — pick an extension on the left, then click an app on the right.");
        gtk_widget_add_css_class(empty, "ase-cfg-hint");
        gtk_label_set_xalign(GTK_LABEL(empty), 0.0f);
        gtk_box_append(GTK_BOX(state->chips_box), empty);
        return;
    }
    for (const auto& [ext, desktop_id] : all) {
        gtk_box_append(GTK_BOX(state->chips_box), build_chip(ext, desktop_id, state));
    }
}

void update_hint(CockpitState* state) {
    if (!state->hint_label) return;
    std::string text;
    if (state->selected_extension.empty()) {
        text = "Pick an extension on the left to start.";
    } else {
        const std::string id = state->store->lookup(state->selected_extension);
        if (id.empty()) {
            text = "." + state->selected_extension
                 + "  →  click an application on the right to map it.";
        } else {
            const AppEntry app = app_catalog::find_by_id(id);
            text = "." + state->selected_extension + "  →  "
                 + (app.name.empty() ? id : app.name)
                 + "   (click another app to overwrite)";
        }
    }
    gtk_label_set_text(GTK_LABEL(state->hint_label), text.c_str());
}

GtkWidget* build_app_picker_row(const AppEntry& entry) {
    GtkWidget* row = gtk_list_box_row_new();
    gtk_widget_add_css_class(row, "ase-cfg-app-row");

    GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, t::space_md);

    // Use the NerdFont desktop icon as a uniform glyph for every row. The
    // per-app icons would have to come from gnome-icon-theme which is
    // forbidden by INST_ASE_ICO_SYS — and a uniform glyph is more cockpit
    // anyway (no visual jitter between heterogeneous app icons).
    GtkWidget* icon = ase::explorer::icons::make_glyph_label(
        ase::ui_icons::ICON_DESKTOP,
        ase::colors::PANEL_CYAN & 0xFFFFFF,
        ase::explorer::icons::ICON_FONT_SIZE);
    gtk_box_append(GTK_BOX(hbox), icon);

    GtkWidget* text_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(text_box, TRUE);

    GtkWidget* name_label = gtk_label_new(entry.name.c_str());
    gtk_widget_add_css_class(name_label, "ase-cfg-app-name");
    gtk_label_set_xalign(GTK_LABEL(name_label), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(name_label), PANGO_ELLIPSIZE_END);
    gtk_box_append(GTK_BOX(text_box), name_label);

    GtkWidget* id_label = gtk_label_new(entry.desktop_id.c_str());
    gtk_widget_add_css_class(id_label, "ase-cfg-app-id");
    gtk_label_set_xalign(GTK_LABEL(id_label), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(id_label), PANGO_ELLIPSIZE_END);
    gtk_box_append(GTK_BOX(text_box), id_label);

    gtk_box_append(GTK_BOX(hbox), text_box);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), hbox);

    return row;
}

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

    // ── LEFT pane: scanned extensions ──
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

    // ── DIVIDER ──
    GtkWidget* divider = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(divider, "ase-cfg-pane-divider");
    gtk_widget_set_size_request(divider, 1, -1);

    // ── RIGHT pane: app picker (identical structure) ──
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

    // ── BOTTOM: ACTIVE MAPPINGS chip strip (full width) ──
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

    // Pin shared_ptr to the page so all callbacks remain valid.
    auto* state_holder = new std::shared_ptr<CockpitState>(state);
    g_object_set_data_full(
        G_OBJECT(page),
        "ase-cockpit-state",
        state_holder,
        +[](gpointer p) { delete static_cast<std::shared_ptr<CockpitState>*>(p); });

    return page;
}

}  // namespace

void show(ase::adp::gtk::ApplicationWindow& parent,
          FileAssociations& associations,
          ExplorerSettings& settings,
          const std::string& root_path,
          std::function<void()> on_close)
{
    auto window = ase::adp::adw::Window::create();
    window.set_title("Preferences");
    window.set_default_size(CFG_WIDTH, CFG_HEIGHT);
    window.set_transient_for(parent);
    window.set_modal(false);

    // Escape closes the preferences window.
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
            }), window.native());
        gtk_widget_add_controller(GTK_WIDGET(window.native()), esc);
    }

    auto toolbar = ase::adp::adw::ToolbarView::create();
    auto header  = ase::adp::adw::HeaderBar::create();
    auto stack   = ase::adp::adw::ViewStack::create();

    // Header bar stays minimal — window controls only, NO title widget.
    // The shader-tuner aesthetic puts the tab bar in the body, not in
    // the header (see sha-gate menu.css `.mm-config-tabs`).
    toolbar.add_top_bar(header);

    AdwViewStack* raw_stack = stack.native();

    GtkWidget* display_page      = build_display_page(settings);
    GtkWidget* behavior_page     = build_behavior_page();
    GtkWidget* associations_page = build_associations_page(associations, root_path);

    // No icons on tabs — text only, like the shader tuner. gnome-icon-theme
    // names are forbidden per INST_ASE_ICO_SYS, and adding NerdFont glyphs
    // here would require a custom switcher (AdwViewStackPage takes an icon
    // name string, not a widget).
    adw_view_stack_add_titled(raw_stack, display_page,      "display",      "DISPLAY");
    adw_view_stack_add_titled(raw_stack, behavior_page,     "behavior",     "BEHAVIOR");
    adw_view_stack_add_titled(raw_stack, associations_page, "associations", "ASSOCIATIONS");

    // Build the body: tab bar (left-aligned) above the stack, both inside
    // a vertical GtkBox. The whole assembly becomes the toolbar's content.
    GtkWidget* body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(body, "ase-cfg-window");
    gtk_widget_set_hexpand(body, TRUE);
    gtk_widget_set_vexpand(body, TRUE);

    AdwViewSwitcher* sw = ADW_VIEW_SWITCHER(adw_view_switcher_new());
    adw_view_switcher_set_stack(sw, raw_stack);
    adw_view_switcher_set_policy(sw, ADW_VIEW_SWITCHER_POLICY_WIDE);

    GtkWidget* tab_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(tab_bar, "ase-cfg-tab-bar");
    gtk_widget_set_halign(tab_bar, GTK_ALIGN_START);
    gtk_widget_set_hexpand(tab_bar, TRUE);
    gtk_box_append(GTK_BOX(tab_bar), GTK_WIDGET(sw));
    gtk_box_append(GTK_BOX(body), tab_bar);

    GtkWidget* stack_widget = GTK_WIDGET(raw_stack);
    gtk_widget_set_hexpand(stack_widget, TRUE);
    gtk_widget_set_vexpand(stack_widget, TRUE);
    gtk_box_append(GTK_BOX(body), stack_widget);

    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar.native_widget()), body);
    adw_window_set_content(ADW_WINDOW(window.native()), toolbar.native_widget());

    // Wire the on_close hook so callers can refresh state that depends on
    // the now-mutated FileAssociations store (e.g. the tree view's mapping
    // indicator dots). The std::function is heap-allocated and owned by the
    // window via g_object_set_data_full.
    if (on_close) {
        auto* heap_cb = new std::function<void()>(std::move(on_close));
        g_object_set_data_full(
            G_OBJECT(window.native()),
            "ase-on-close",
            heap_cb,
            +[](gpointer p) {
                auto* cb = static_cast<std::function<void()>*>(p);
                if (cb && *cb) (*cb)();
                delete cb;
            });
    }

    window.present();
}

}  // namespace ase::explorer::settings_dialog

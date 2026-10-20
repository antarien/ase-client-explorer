/**
 * ASE EXPLORER NATIVE - ASSOCIATIONS VIEW
 *
 * @file        settings_assoc_view.cpp
 * @brief       Draws the state of the Associations page: extension rows, chips, hint, app rows
 * @description Each function here reads a CockpitState and builds the widgets for it; none
 *              decides what the user selected. The extension list shows every scanned
 *              extension with the application it maps to, the chip strip every mapping of
 *              the FileAssociations store with a remove button, the hint line the next step.
 *
 *              Every spacing value is sourced from ase::theme::* (design_tokens.hpp), every
 *              colour from ase::colors::* (colors.hpp); the .ase-cfg-* CSS classes are
 *              defined in theme.cpp.
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

#include <explorer/settings_assoc_view.hpp>

#include <explorer/file_associations.hpp>
#include <explorer/icons.hpp>

#include "colors.hpp"
#include "design_tokens.hpp"
#include "ui_icons.hpp"

#include <gtk/gtk.h>

#include <string>

namespace ase::explorer::settings_page {

namespace {

namespace t = ase::theme;

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

}  // namespace

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

}  // namespace ase::explorer::settings_page

/**
 * ASE EXPLORER NATIVE - SETTINGS DIALOG
 *
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
 *                               app picker over every installed XDG app),
 *                               built in settings_assoc_page.cpp
 *
 *              Every spacing / font-size / padding value is sourced from
 *              ase::theme::* (design_tokens.hpp). The visual look (uppercase
 *              tabs in MENU_RED, monospace, MENU_RED section strips)
 *              is provided by the .ase-cfg-* CSS classes in theme.cpp.
 *
 *              The root folder is chosen through the folder picker of
 *              ase-adp-gtk (usecase:101129 - choose a folder), the one picker
 *              ase-client-viewer opens as well.
 *
 * @module      ase-client-explorer
 * @layer       5 (Clients)
 * @category    render/ui/widget
 * @usecase     usecase:101131 - set explorer preferences
 * @created     2026-04-13
 * @modified    2026-10-19
 * @version     1.1.0
 *
 * CLIENT NATIVE COMPLIANCE
 *
 * [ ] English only - identifiers, comments, messages
 * [ ] META fields complete, @layer 5 (Section 0)
 * [ ] Headings are bare capital lines, cuts are empty lines (Section 0.1)
 * [ ] Form defined in INST_ASE_LINT.md Section 23
 */

#include <explorer/settings_dialog.hpp>

#include <explorer/explorer_settings.hpp>
#include <explorer/settings_page.hpp>

#include <ase/adp/adw/adw.hpp>
#include <ase/adp/adw/cockpit.hpp>
#include <ase/adp/adw/folder_picker.hpp>

#include "design_tokens.hpp"

#include <adwaita.h>
#include <gtk/gtk.h>

#include <string>

namespace ase::explorer::settings_dialog {

namespace {

namespace t = ase::theme;

// The cockpit cells of ase-adp-gtk (usecase:101134 - lay out settings cells), the same
// ones ase-client-viewer lays its preferences out with.
using ase::adp::adw::cockpit::make_cell;
using ase::adp::adw::cockpit::make_cell_row;
using ase::adp::adw::cockpit::make_dropdown;
using ase::adp::adw::cockpit::make_section_strip;
using ase::adp::adw::cockpit::make_toggle;

// Cockpit window default — explorer-local layout decision, deliberately
// expressed in terms of the official spacing scale so any future global
// scale tweak proportionally adjusts the dialog as well.
constexpr int CFG_WIDTH  = t::space_2xl * 35 + t::space_md;   // 1696
constexpr int CFG_HEIGHT = t::space_2xl * 21 + t::space_xl;   // 1040

// DISPLAY AND BEHAVIOR PAGES

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

// Fires once the user accepts a directory in the picker. `user_data` is the same
// RootBrowseState the browse button carries; the folder picker hands it back untouched.
// A free function rather than a captured closure: GTK and the picker both take a plain
// function plus a data pointer, so a closure would only wrap what the callee unwraps
// again - on_breadcrumb_max_changed_cb and on_default_root_changed_cb above are built
// the same way.
void on_root_chosen_cb(const std::string& chosen, void* user_data) {
    auto* bs = static_cast<RootBrowseState*>(user_data);
    if (!bs || !bs->entry || !bs->settings) return;
    gtk_editable_set_text(GTK_EDITABLE(bs->entry), chosen.c_str());
    bs->settings->set_default_root(chosen);
    bs->settings->save();
}

// Browse-button click handler. Routes to the in-process folder picker of
// ase-adp-gtk instead of GtkFileDialog because the latter (a) requires an
// xdg-desktop-portal FileChooser backend that isn't available on a pure
// Hyprland session, and (b) when it falls back to its own internal
// GtkPathBar-based chooser it triggers a known "gtk_box_remove:
// GTK_IS_BOX (box) failed" cascade and never shows a picker.
//
// The picker is built from raw GTK4 widgets the adapter controls end-to-end.
void on_browse_root_clicked_cb(GtkButton* btn, gpointer user_data) {
    auto* bs = static_cast<RootBrowseState*>(user_data);
    if (!bs || !bs->entry) return;

    GtkRoot* root = gtk_widget_get_root(GTK_WIDGET(btn));
    GtkWindow* parent = (root && GTK_IS_WINDOW(root)) ? GTK_WINDOW(root) : nullptr;

    const char* current = gtk_editable_get_text(GTK_EDITABLE(bs->entry));
    const std::string start = current ? std::string(current) : std::string();

    // bs outlives the picker: it is owned by the browse button, and the button outlives
    // any window the button opened.
    ase::adp::adw::folder_picker::show(parent, start, on_root_chosen_cb, bs);
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

// The close hook as one addressable thing: the function and the data it wants handed
// back. It lives as long as the dialog window and is released by the same destroy notify
// that fires it, so no caller has to think about its lifetime.
struct CloseHook {
    CloseFn fn        = nullptr;
    void*   user_data = nullptr;
};

}  // namespace

void show(ase::adp::gtk::ApplicationWindow& parent,
          FileAssociations& associations,
          ExplorerSettings& settings,
          const std::string& root_path,
          CloseFn on_close,
          void* on_close_data)
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
    GtkWidget* associations_page = settings_page::build_associations_page(associations, root_path);

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

    // Wire the on_close hook so callers can refresh state that depends on the
    // now-mutated FileAssociations store (e.g. the tree view's mapping indicator dots).
    // The hook pair is GLib-allocated and owned by the window; the destroy notify both
    // fires it and releases it, so it cannot outlive the dialog and cannot leak.
    if (on_close) {
        auto* hook = static_cast<CloseHook*>(g_malloc0(sizeof(CloseHook)));
        new (hook) CloseHook();
        hook->fn        = on_close;
        hook->user_data = on_close_data;
        g_object_set_data_full(
            G_OBJECT(window.native()),
            "ase-on-close",
            hook,
            +[](gpointer p) {
                auto* h = static_cast<CloseHook*>(p);
                if (!h) return;
                if (h->fn) h->fn(h->user_data);
                h->~CloseHook();
                g_free(h);
            });
    }

    window.present();
}

}  // namespace ase::explorer::settings_dialog

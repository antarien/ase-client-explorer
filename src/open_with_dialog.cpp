/**
 * @file        open_with_dialog.cpp
 * @brief       Implementation for open_with_dialog.hpp
 * @description Pure libadwaita / GTK C API implementation. Builds an AdwWindow
 *              shell directly so the dialog can host a GtkListBox (which the
 *              ase::adp::adw wrapper layer does not yet expose). All state lives in
 *              a heap-allocated DialogState attached to the window's "destroy"
 *              signal so it is freed exactly when the window goes away.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <explorer/open_with_dialog.hpp>

#include <explorer/app_catalog.hpp>
#include <explorer/file_associations.hpp>
#include <explorer/icons.hpp>

#include <ase/utils/fs.hpp>

#include <gtkmm/applicationwindow.h>

#include "colors.hpp"
#include "ui_icons.hpp"

#include <adwaita.h>
#include <gtk/gtk.h>

#include <cctype>
#include <string>
#include <vector>

namespace ase::explorer::open_with_dialog {

namespace {

constexpr int DIALOG_WIDTH  = 560;
constexpr int DIALOG_HEIGHT = 520;

struct DialogState {
    GtkWidget* window        = nullptr;
    GtkWidget* list_box      = nullptr;
    GtkWidget* search_entry  = nullptr;
    GtkWidget* always_check  = nullptr;
    GtkWidget* open_button   = nullptr;

    std::vector<AppEntry>    entries;
    std::string              search_text;
    std::string              file_path;
    std::string              extension;
    FileAssociations*        store = nullptr;
};

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string extract_extension(const std::string& path) {
    const std::string name = ase::utils::fs::filename_of(path);
    auto dot = name.rfind('.');
    if (dot == std::string::npos || dot == 0) return {};
    return lower(name.substr(dot + 1));
}

GtkWidget* build_app_row(const AppEntry& entry) {
    GtkWidget* row = gtk_list_box_row_new();

    GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_start (hbox, 8);
    gtk_widget_set_margin_end   (hbox, 8);
    gtk_widget_set_margin_top   (hbox, 4);
    gtk_widget_set_margin_bottom(hbox, 4);

    GtkWidget* icon = ase::explorer::icons::make_glyph_label(
        ase::ui_icons::ICON_DESKTOP,
        ase::colors::PANEL_CYAN & 0xFFFFFF,
        ase::explorer::icons::ICON_FONT_SIZE);
    gtk_box_append(GTK_BOX(hbox), icon);

    GtkWidget* text_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(text_box, TRUE);

    GtkWidget* name_label = gtk_label_new(entry.name.c_str());
    gtk_widget_add_css_class(name_label, "ase-openwith-app-name");
    gtk_label_set_xalign(GTK_LABEL(name_label), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(name_label), PANGO_ELLIPSIZE_END);
    gtk_box_append(GTK_BOX(text_box), name_label);

    if (!entry.exec.empty()) {
        GtkWidget* exec_label = gtk_label_new(entry.exec.c_str());
        gtk_widget_add_css_class(exec_label, "ase-openwith-app-exec");
        gtk_label_set_xalign(GTK_LABEL(exec_label), 0.0f);
        gtk_label_set_ellipsize(GTK_LABEL(exec_label), PANGO_ELLIPSIZE_END);
        gtk_box_append(GTK_BOX(text_box), exec_label);
    }

    gtk_box_append(GTK_BOX(hbox), text_box);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), hbox);
    return row;
}

gboolean filter_row_cb(GtkListBoxRow* row, gpointer user_data) {
    auto* state = static_cast<DialogState*>(user_data);
    if (state->search_text.empty()) return TRUE;

    auto* entry_ptr = static_cast<AppEntry*>(g_object_get_data(G_OBJECT(row), "ase-app-entry"));
    if (!entry_ptr) return FALSE;

    const std::string needle = lower(state->search_text);
    return (lower(entry_ptr->name).find(needle) != std::string::npos
         || lower(entry_ptr->exec).find(needle) != std::string::npos
         || lower(entry_ptr->desktop_id).find(needle) != std::string::npos)
         ? TRUE : FALSE;
}

void launch_selected(DialogState* state) {
    GtkListBoxRow* row = gtk_list_box_get_selected_row(GTK_LIST_BOX(state->list_box));
    if (!row) return;
    auto* entry_ptr = static_cast<AppEntry*>(g_object_get_data(G_OBJECT(row), "ase-app-entry"));
    if (!entry_ptr) return;

    if (state->store && state->always_check
        && gtk_check_button_get_active(GTK_CHECK_BUTTON(state->always_check))
        && !state->extension.empty())
    {
        state->store->set(state->extension, entry_ptr->desktop_id);
        state->store->save();
    }

    app_catalog::launch(entry_ptr->desktop_id, state->file_path);
    gtk_window_close(GTK_WINDOW(state->window));
}

void on_search_changed_cb(GtkSearchEntry* entry, gpointer user_data) {
    auto* state = static_cast<DialogState*>(user_data);
    const char* text = gtk_editable_get_text(GTK_EDITABLE(entry));
    state->search_text = text ? text : "";
    gtk_list_box_invalidate_filter(GTK_LIST_BOX(state->list_box));
}

void on_row_activated_cb(GtkListBox*, GtkListBoxRow*, gpointer user_data) {
    launch_selected(static_cast<DialogState*>(user_data));
}

void on_row_selected_cb(GtkListBox*, GtkListBoxRow* row, gpointer user_data) {
    auto* state = static_cast<DialogState*>(user_data);
    gtk_widget_set_sensitive(state->open_button, row != nullptr);
}

void on_open_clicked_cb(GtkButton*, gpointer user_data) {
    launch_selected(static_cast<DialogState*>(user_data));
}

void on_cancel_clicked_cb(GtkButton*, gpointer user_data) {
    auto* state = static_cast<DialogState*>(user_data);
    gtk_window_close(GTK_WINDOW(state->window));
}

void on_window_destroy_cb(GtkWidget*, gpointer user_data) {
    delete static_cast<DialogState*>(user_data);
}

}  // namespace

void show(ase::adp::gtk::ApplicationWindow& parent,
          const std::string& file_path,
          FileAssociations& store)
{
    if (file_path.empty()) return;

    auto* state = new DialogState();
    state->file_path = file_path;
    state->extension = extract_extension(file_path);
    state->store     = &store;

    const std::string mime = app_catalog::mime_type_for_path(file_path);
    state->entries = app_catalog::for_mime_type(mime);
    if (state->entries.empty()) {
        state->entries = app_catalog::all();
    }

    GtkWidget* window = adw_window_new();
    state->window = window;
    gtk_widget_add_css_class(window, "ase-openwith");
    gtk_window_set_title(GTK_WINDOW(window), "Open With");
    gtk_window_set_default_size(GTK_WINDOW(window), DIALOG_WIDTH, DIALOG_HEIGHT);
    gtk_window_set_modal(GTK_WINDOW(window), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(window),
        GTK_WINDOW(parent.native_widget()->gobj()));

    // Escape closes the dialog.
    GtkEventController* esc = gtk_event_controller_key_new();
    g_signal_connect(esc, "key-pressed",
        G_CALLBACK(+[](GtkEventControllerKey*, guint keyval, guint,
                       GdkModifierType, gpointer w) -> gboolean {
            if (keyval == GDK_KEY_Escape) {
                gtk_window_close(GTK_WINDOW(w));
                return TRUE;
            }
            return FALSE;
        }), window);
    gtk_widget_add_controller(window, esc);

    GtkWidget* toolbar = adw_toolbar_view_new();

    GtkWidget* header = adw_header_bar_new();
    const std::string title_text = state->extension.empty()
        ? std::string("OPEN WITH")
        : ("OPEN ." + state->extension + " WITH");
    GtkWidget* title_label = gtk_label_new(title_text.c_str());
    gtk_widget_add_css_class(title_label, "ase-openwith-header");
    adw_header_bar_set_title_widget(ADW_HEADER_BAR(header), title_label);
    adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar), header);

    GtkWidget* body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start (body, 16);
    gtk_widget_set_margin_end   (body, 16);
    gtk_widget_set_margin_top   (body, 16);
    gtk_widget_set_margin_bottom(body, 16);

    GtkWidget* search_entry = gtk_search_entry_new();
    state->search_entry = search_entry;
    gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(search_entry), "Search applications…");
    g_signal_connect(search_entry, "search-changed", G_CALLBACK(on_search_changed_cb), state);
    gtk_box_append(GTK_BOX(body), search_entry);

    GtkWidget* scroller = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroller, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget* list_box = gtk_list_box_new();
    state->list_box = list_box;
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(list_box), GTK_SELECTION_BROWSE);
    gtk_list_box_set_filter_func(GTK_LIST_BOX(list_box), filter_row_cb, state, nullptr);
    g_signal_connect(list_box, "row-activated", G_CALLBACK(on_row_activated_cb), state);
    g_signal_connect(list_box, "row-selected",  G_CALLBACK(on_row_selected_cb),  state);

    for (auto& entry : state->entries) {
        GtkWidget* row = build_app_row(entry);
        g_object_set_data(G_OBJECT(row), "ase-app-entry", &entry);
        gtk_list_box_append(GTK_LIST_BOX(list_box), row);
    }

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), list_box);
    gtk_box_append(GTK_BOX(body), scroller);

    // Bottom row: "Always use this" + Cancel + Open
    GtkWidget* bottom = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    GtkWidget* always_check = nullptr;
    if (!state->extension.empty()) {
        const std::string label = "Always use this for ." + state->extension;
        always_check = gtk_check_button_new_with_label(label.c_str());
        state->always_check = always_check;
        gtk_widget_set_hexpand(always_check, TRUE);
        gtk_box_append(GTK_BOX(bottom), always_check);
    } else {
        GtkWidget* spacer = gtk_label_new("");
        gtk_widget_set_hexpand(spacer, TRUE);
        gtk_box_append(GTK_BOX(bottom), spacer);
    }

    GtkWidget* cancel_button = gtk_button_new_with_label("Cancel");
    g_signal_connect(cancel_button, "clicked", G_CALLBACK(on_cancel_clicked_cb), state);
    gtk_box_append(GTK_BOX(bottom), cancel_button);

    GtkWidget* open_button = gtk_button_new_with_label("Open");
    state->open_button = open_button;
    gtk_widget_add_css_class(open_button, "suggested-action");
    gtk_widget_set_sensitive(open_button, FALSE);
    g_signal_connect(open_button, "clicked", G_CALLBACK(on_open_clicked_cb), state);
    gtk_box_append(GTK_BOX(bottom), open_button);

    gtk_box_append(GTK_BOX(body), bottom);

    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar), body);
    adw_window_set_content(ADW_WINDOW(window), toolbar);

    g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy_cb), state);

    gtk_window_present(GTK_WINDOW(window));
    gtk_widget_grab_focus(search_entry);
}

}  // namespace ase::explorer::open_with_dialog

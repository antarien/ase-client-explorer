/**
 * @file        folder_picker.cpp
 * @brief       Implementation for folder_picker.hpp
 * @description AdwWindow + GtkListBox + std::filesystem. No GtkFileDialog,
 *              no GtkFileChooser, no GtkPathBar, no portal dependency. Every
 *              widget is created and managed by us.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <explorer/folder_picker.hpp>

#include "design_tokens.hpp"

#include <adwaita.h>
#include <gtk/gtk.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <system_error>

namespace ase::explorer::folder_picker {

namespace fs = std::filesystem;
namespace t  = ase::theme;

namespace {

struct PickerState {
    GtkWidget*                                  window      = nullptr;
    GtkWidget*                                  path_entry  = nullptr;
    GtkWidget*                                  listbox     = nullptr;
    GtkWidget*                                  open_button = nullptr;
    std::string                                 current;
    std::function<void(const std::string&)>     on_selected;
};

void rebuild_listbox(PickerState* state);

std::string sanitise_initial(const std::string& start) {
    std::error_code ec;
    if (!start.empty() && fs::exists(start, ec) && fs::is_directory(start, ec)) {
        return fs::absolute(start, ec).lexically_normal().string();
    }
    if (const char* home = std::getenv("HOME"); home && *home) return home;
    return "/";
}

void navigate_to(PickerState* state, const std::string& path) {
    std::error_code ec;
    if (!fs::exists(path, ec) || !fs::is_directory(path, ec)) return;
    state->current = fs::absolute(path, ec).lexically_normal().string();
    gtk_editable_set_text(GTK_EDITABLE(state->path_entry), state->current.c_str());
    rebuild_listbox(state);
}

void on_row_activated_cb(GtkListBox*, GtkListBoxRow* row, gpointer user_data) {
    auto* state = static_cast<PickerState*>(user_data);
    if (!row) return;
    const char* name = static_cast<const char*>(g_object_get_data(G_OBJECT(row), "ase-folder-name"));
    if (!name) return;
    navigate_to(state, (fs::path(state->current) / name).string());
}

void on_path_entry_activate_cb(GtkEntry* entry, gpointer user_data) {
    auto* state = static_cast<PickerState*>(user_data);
    const char* text = gtk_editable_get_text(GTK_EDITABLE(entry));
    if (text && *text) navigate_to(state, text);
}

void on_up_clicked_cb(GtkButton*, gpointer user_data) {
    auto* state = static_cast<PickerState*>(user_data);
    fs::path p = fs::path(state->current).parent_path();
    if (p.empty()) p = "/";
    navigate_to(state, p.string());
}

void on_home_clicked_cb(GtkButton*, gpointer user_data) {
    auto* state = static_cast<PickerState*>(user_data);
    if (const char* home = std::getenv("HOME"); home && *home) navigate_to(state, home);
}

void on_open_clicked_cb(GtkButton*, gpointer user_data) {
    auto* state = static_cast<PickerState*>(user_data);
    // Use whatever the entry currently shows (matches user expectation if
    // they typed a path manually rather than navigating).
    const char* text = gtk_editable_get_text(GTK_EDITABLE(state->path_entry));
    std::string final_path = text && *text ? std::string(text) : state->current;
    std::error_code ec;
    if (!fs::exists(final_path, ec) || !fs::is_directory(final_path, ec)) return;
    if (state->on_selected) state->on_selected(final_path);
    gtk_window_close(GTK_WINDOW(state->window));
}

void on_cancel_clicked_cb(GtkButton*, gpointer user_data) {
    auto* state = static_cast<PickerState*>(user_data);
    gtk_window_close(GTK_WINDOW(state->window));
}

void on_window_destroy_cb(GtkWidget*, gpointer user_data) {
    delete static_cast<PickerState*>(user_data);
}

GtkWidget* build_row(const std::string& name) {
    GtkWidget* row = gtk_list_box_row_new();
    gtk_widget_add_css_class(row, "ase-cfg-app-row");

    GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, t::space_sm);

    GtkWidget* label = gtk_label_new(name.c_str());
    gtk_widget_add_css_class(label, "ase-cfg-app-name");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_box_append(GTK_BOX(hbox), label);

    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), hbox);

    char* name_dup = g_strdup(name.c_str());
    g_object_set_data_full(G_OBJECT(row), "ase-folder-name", name_dup, g_free);
    return row;
}

gboolean on_escape_key_cb(GtkEventControllerKey*, guint keyval, guint,
                          GdkModifierType, gpointer user_data)
{
    if (keyval == GDK_KEY_Escape) {
        gtk_window_close(GTK_WINDOW(user_data));
        return TRUE;
    }
    return FALSE;
}

void rebuild_listbox(PickerState* state) {
    if (!state || !state->listbox) return;

    GtkWidget* child = gtk_widget_get_first_child(state->listbox);
    while (child) {
        GtkWidget* next = gtk_widget_get_next_sibling(child);
        gtk_list_box_remove(GTK_LIST_BOX(state->listbox), child);
        child = next;
    }

    std::vector<std::string> dirs;
    std::error_code ec;
    fs::directory_iterator it(state->current, fs::directory_options::skip_permission_denied, ec);
    if (!ec) {
        for (const auto& entry : it) {
            std::error_code ec2;
            if (entry.is_directory(ec2)) {
                dirs.push_back(entry.path().filename().string());
            }
        }
    }
    std::sort(dirs.begin(), dirs.end(),
              [](const std::string& a, const std::string& b) {
                  std::string la = a, lb = b;
                  std::transform(la.begin(), la.end(), la.begin(), ::tolower);
                  std::transform(lb.begin(), lb.end(), lb.begin(), ::tolower);
                  return la < lb;
              });

    for (const auto& d : dirs) {
        gtk_list_box_append(GTK_LIST_BOX(state->listbox), build_row(d));
    }
}

}  // namespace

void show(GtkWindow* parent,
          const std::string& start_path,
          std::function<void(const std::string&)> on_selected)
{
    auto* state = new PickerState();
    state->current     = sanitise_initial(start_path);
    state->on_selected = std::move(on_selected);

    GtkWidget* window = adw_window_new();
    state->window = window;
    gtk_widget_add_css_class(window, "ase-cfg-window");
    gtk_window_set_title(GTK_WINDOW(window), "Choose a folder");
    gtk_window_set_default_size(GTK_WINDOW(window),
        t::space_2xl * 14,   // ~672 px
        t::space_2xl * 12);  // ~576 px
    gtk_window_set_modal(GTK_WINDOW(window), TRUE);
    if (parent) gtk_window_set_transient_for(GTK_WINDOW(window), parent);

    // Escape closes the picker.
    GtkEventController* esc = gtk_event_controller_key_new();
    g_signal_connect(esc, "key-pressed", G_CALLBACK(on_escape_key_cb), window);
    gtk_widget_add_controller(window, esc);

    GtkWidget* toolbar = adw_toolbar_view_new();
    GtkWidget* header  = adw_header_bar_new();
    adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar), header);

    GtkWidget* body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(body, "ase-cfg-pane");

    // Section strip header — same uppercase tinted style as the settings dialog.
    GtkWidget* section = gtk_label_new("CHOOSE FOLDER");
    gtk_widget_add_css_class(section, "ase-cfg-section-title");
    gtk_label_set_xalign(GTK_LABEL(section), 0.0f);
    gtk_widget_set_hexpand(section, TRUE);
    gtk_box_append(GTK_BOX(body), section);

    // ── Top row: ↑ Home  [path entry] ──
    GtkWidget* nav_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, t::space_sm);
    gtk_widget_set_margin_start (nav_row, t::space_md);
    gtk_widget_set_margin_end   (nav_row, t::space_md);
    gtk_widget_set_margin_top   (nav_row, t::space_md);
    gtk_widget_set_margin_bottom(nav_row, t::space_sm);

    GtkWidget* up_btn = gtk_button_new_with_label("↑");
    gtk_widget_set_tooltip_text(up_btn, "Parent directory");
    g_signal_connect(up_btn, "clicked", G_CALLBACK(on_up_clicked_cb), state);
    gtk_box_append(GTK_BOX(nav_row), up_btn);

    GtkWidget* home_btn = gtk_button_new_with_label("HOME");
    g_signal_connect(home_btn, "clicked", G_CALLBACK(on_home_clicked_cb), state);
    gtk_box_append(GTK_BOX(nav_row), home_btn);

    GtkWidget* path_entry = gtk_entry_new();
    state->path_entry = path_entry;
    gtk_widget_set_hexpand(path_entry, TRUE);
    gtk_widget_set_halign(path_entry, GTK_ALIGN_FILL);
    gtk_editable_set_text(GTK_EDITABLE(path_entry), state->current.c_str());
    g_signal_connect(path_entry, "activate", G_CALLBACK(on_path_entry_activate_cb), state);
    gtk_box_append(GTK_BOX(nav_row), path_entry);

    gtk_box_append(GTK_BOX(body), nav_row);

    // ── Scrolled directory listing ──
    GtkWidget* scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_widget_set_hexpand(scroll, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget* listbox = gtk_list_box_new();
    state->listbox = listbox;
    gtk_widget_add_css_class(listbox, "ase-cfg-window");
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(listbox), GTK_SELECTION_BROWSE);
    g_signal_connect(listbox, "row-activated", G_CALLBACK(on_row_activated_cb), state);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), listbox);
    gtk_box_append(GTK_BOX(body), scroll);

    // ── Footer: Cancel + Open ──
    GtkWidget* footer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, t::space_sm);
    gtk_widget_set_margin_start (footer, t::space_md);
    gtk_widget_set_margin_end   (footer, t::space_md);
    gtk_widget_set_margin_top   (footer, t::space_sm);
    gtk_widget_set_margin_bottom(footer, t::space_md);
    gtk_widget_set_halign(footer, GTK_ALIGN_END);

    GtkWidget* cancel_btn = gtk_button_new_with_label("Cancel");
    g_signal_connect(cancel_btn, "clicked", G_CALLBACK(on_cancel_clicked_cb), state);
    gtk_box_append(GTK_BOX(footer), cancel_btn);

    GtkWidget* open_btn = gtk_button_new_with_label("Open");
    state->open_button = open_btn;
    gtk_widget_add_css_class(open_btn, "suggested-action");
    g_signal_connect(open_btn, "clicked", G_CALLBACK(on_open_clicked_cb), state);
    gtk_box_append(GTK_BOX(footer), open_btn);

    gtk_box_append(GTK_BOX(body), footer);

    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar), body);
    adw_window_set_content(ADW_WINDOW(window), toolbar);

    g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy_cb), state);

    rebuild_listbox(state);
    gtk_window_present(GTK_WINDOW(window));
}

}  // namespace ase::explorer::folder_picker

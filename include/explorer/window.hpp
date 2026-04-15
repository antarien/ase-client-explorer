#pragma once

/**
 * @file        window.hpp
 * @brief       ExplorerWindow - orchestrator that composes every feature
 * @description Owns one instance of each UI feature (TreeView, SearchBar,
 *              Breadcrumb, ContextMenu, KeyboardShortcuts, FileWatcher)
 *              plus the top-level ApplicationWindow. build_ui() wires
 *              everything together (including the drag-source controller
 *              inline on the list view, matching the single-file explorer
 *              layout); load_root() switches the current directory;
 *              refresh() reloads without moving.
 *
 *              This class is the ONLY place that knows about all the feature
 *              slices - each feature file is independent and delegates back
 *              to the window via sigc::slot callbacks installed during build.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <explorer/breadcrumb.hpp>
#include <explorer/context_menu.hpp>
#include <explorer/explorer_settings.hpp>
#include <explorer/file_associations.hpp>
#include <explorer/file_watcher.hpp>
#include <explorer/keyboard_shortcuts.hpp>
#include <explorer/search_bar.hpp>
#include <explorer/tree_view.hpp>

#include <ase/adp/gtk/application.hpp>

#include <string>

typedef struct _GtkWidget GtkWidget;

namespace ase::explorer {

class ExplorerWindow {
public:
    explicit ExplorerWindow(ase::adp::gtk::ApplicationWindow window);

    /** Assemble the full UI: header, breadcrumb, tree, controllers, shortcuts. */
    void build_ui();

    /** Load a directory as the new root (triggers tree rebuild + breadcrumb update). */
    void load_root(const std::string& path);

    /** Load whichever root path is persisted in ExplorerSettings. */
    void load_default_root();

    /** Rescan the current root and rebuild the tree without moving. */
    void refresh();

    /** Forward to the underlying ApplicationWindow. */
    void present();

private:
    // Handlers invoked by the feature slices via their stored slots.
    void handle_activate_selection();
    void handle_file_activated(const std::string& path);
    void handle_right_click_open_with();
    void handle_right_click_open_terminal();
    void handle_right_click_reveal();
    void handle_copy_path(bool relative);
    void handle_search_toggle();
    void handle_escape_close_search();
    void handle_filter_changed(const std::string& text);

    /**
     * Type-ahead search: if a printable key is pressed anywhere in the
     * main window with no modifier and the search entry is currently
     * hidden, open the search entry and seed it with that character.
     * Returns true when the keystroke was consumed.
     */
    bool handle_type_ahead(unsigned keyval, unsigned state);

    ase::adp::gtk::ApplicationWindow m_window;
    std::string m_root_path;

    TreeView          m_tree_view;
    SearchBar         m_search_bar;
    Breadcrumb        m_breadcrumb;
    ContextMenu       m_context_menu;
    KeyboardShortcuts m_shortcuts;
    FileWatcher       m_file_watcher;
    FileAssociations  m_file_associations = FileAssociations::load();
    ExplorerSettings  m_settings          = ExplorerSettings::load();

    // Raw widget handle to the "ASE Explorer" title label in the header
    // bar. Owned by the header (managed widget) but stored here so the
    // search-toggle handler can hide it when the search entry expands to
    // fill the header.
    GtkWidget*        m_title_label_native = nullptr;
};

}  // namespace ase::explorer

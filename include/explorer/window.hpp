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

namespace ase::explorer {

class ExplorerWindow {
public:
    explicit ExplorerWindow(ase::gtk::ApplicationWindow window);

    /** Assemble the full UI: header, breadcrumb, tree, controllers, shortcuts. */
    void build_ui();

    /** Load a directory as the new root (triggers tree rebuild + breadcrumb update). */
    void load_root(const std::string& path);

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

    ase::gtk::ApplicationWindow m_window;
    std::string m_root_path;

    TreeView          m_tree_view;
    SearchBar         m_search_bar;
    Breadcrumb        m_breadcrumb;
    ContextMenu       m_context_menu;
    KeyboardShortcuts m_shortcuts;
    FileWatcher       m_file_watcher;
    FileAssociations  m_file_associations = FileAssociations::load();
    ExplorerSettings  m_settings          = ExplorerSettings::load();
};

}  // namespace ase::explorer

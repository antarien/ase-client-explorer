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
#include <explorer/git_scan_pool.hpp>
#include <explorer/git_status.hpp>
#include <explorer/keyboard_shortcuts.hpp>
#include <explorer/search_bar.hpp>
#include <explorer/tree_view.hpp>

#include <ase/adp/gtk/application.hpp>
#include <ase/adp/libgit2/init.hpp>

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
    void handle_right_click_delete();
    void handle_copy_path(bool relative);
    void handle_search_toggle();
    void handle_escape_close_search();
    void handle_filter_changed(const std::string& text);
    void handle_vcs_filter_toggle();
    void handle_status_updated();

    /**
     * Type-ahead search: if a printable key is pressed anywhere in the
     * main window with no modifier and the search entry is currently
     * hidden, open the search entry and seed it with that character.
     * Returns true when the keystroke was consumed.
     */
    bool handle_type_ahead(unsigned keyval, unsigned state);

    ase::adp::gtk::ApplicationWindow m_window;
    std::string m_root_path;

    // libgit2 lives for the lifetime of the explorer window. Init must
    // happen BEFORE the scan pool is constructed (workers will call
    // libgit2 from background threads), and shutdown must happen AFTER
    // the pool joins. Member declaration order ensures both: m_libgit2
    // is destroyed last, m_scan_pool first.
    ase::adp::libgit2::Library m_libgit2;
    git::StatusCache           m_status_cache;
    git::ScanPool              m_scan_pool{m_status_cache};

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

    // Raw handle to the VCS-filter toggle button so handle_vcs_filter_toggle
    // can flip its CSS class for the pressed-look without re-finding it.
    GtkWidget*        m_btn_git_native = nullptr;
    bool              m_vcs_filter_active = false;
};

}  // namespace ase::explorer

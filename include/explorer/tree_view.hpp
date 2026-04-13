#pragma once

/**
 * @file        tree_view.hpp
 * @brief       Hierarchical file tree view backed by DirectoryList
 * @description Owns the list view widget, the TreeListModel chain, the
 *              SingleSelection and the ListItemFactory that renders each
 *              row (icon + name + optional submodule badge). The window
 *              attaches click/drag/key controllers to list_view() after
 *              build(), and triggers repopulation via populate() whenever
 *              the root changes or a refresh is requested.
 *
 *              Submodule metadata is cached per path so repeated bind()
 *              calls during scrolling stay cheap.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <explorer/submodule.hpp>

#include <ase/gtk/list_view.hpp>
#include <ase/gtk/tree.hpp>

#include <gtkmm/multiselection.h>
#include <glibmm/refptr.h>

#include <sigc++/slot.h>

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ase::explorer {

class TreeView {
public:
    TreeView();

    /** The list view widget - pack into a ScrolledWindow and attach controllers. */
    ase::gtk::ListView& list_view() noexcept { return *m_list_view; }

    /** Rebuild the model from root_path; caches submodule metadata internally. */
    void populate(const std::string& root_path);

    /** Returns the FileInfo of the first-selected row, empty if none. */
    ase::gtk::FileInfo selected_file_info() const;

    /** Returns the full path of the first-selected row, or empty string. */
    std::string selected_path() const;

    /**
     * Returns the full paths of ALL currently-selected rows in tree order.
     * Used by the drag source so multi-selection drops carry every marked
     * file/folder, not just the focused one.
     */
    std::vector<std::string> selected_paths() const;

    /** Activate the current selection: open file or toggle folder expansion. */
    void activate_selection();

    /**
     * Recursively expand or collapse the currently-selected row. When expanding,
     * every descendant directory is expanded as well so the whole subtree is
     * visible in one click. Returns true if anything changed.
     */
    bool toggle_recursive_expand_selected();

    /**
     * Navigate to an absolute filesystem path WITHIN the currently-loaded
     * root. Walks the flattened TreeListModel, expands every ancestor of
     * the target along the way, selects the matching row and scrolls it
     * into view. Used by the breadcrumb bar so clicking a segment jumps
     * to that folder in place instead of reloading the entire tree.
     * Returns true if the target was found and selected.
     */
    bool navigate_to(const std::string& target_path);

    /**
     * Install a listener fired whenever the row selection changes. The
     * callback receives the absolute path of the newly-selected row, or an
     * empty string if the selection was cleared. Used by the window to keep
     * the breadcrumb bar in sync with the currently-selected item.
     */
    template <typename Callback>
    void on_selection_changed(Callback&& callback) {
        m_on_selection_changed = sigc::slot<void(const std::string&)>(
            [cb = std::forward<Callback>(callback)](const std::string& path) { cb(path); });
    }

private:
    std::set<std::string> m_submodule_paths;
    std::unordered_map<std::string, submodule::SubmoduleInfo> m_metadata_cache;

    std::unique_ptr<ase::gtk::ListView> m_list_view;
    std::unique_ptr<ase::gtk::TreeListModel> m_tree_model;
    // Gtk::MultiSelection lets the user extend the selection with Shift-click
    // and Ctrl-click out of the box. Held as a raw gtkmm refptr because the
    // adapter currently only wraps Gtk::SingleSelection; no wrapper is
    // needed here since tree_view is the only consumer.
    Glib::RefPtr<Gtk::MultiSelection> m_selection;
    std::unique_ptr<ase::gtk::ListItemFactory> m_factory;

    sigc::slot<void(const std::string&)> m_on_selection_changed;
};

}  // namespace ase::explorer

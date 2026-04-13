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

#include <memory>
#include <set>
#include <string>
#include <unordered_map>

namespace ase::explorer {

class TreeView {
public:
    TreeView();

    /** The list view widget - pack into a ScrolledWindow and attach controllers. */
    ase::gtk::ListView& list_view() noexcept { return *m_list_view; }

    /** Rebuild the model from root_path; caches submodule metadata internally. */
    void populate(const std::string& root_path);

    /** Returns the currently-selected FileInfo (empty if nothing is selected). */
    ase::gtk::FileInfo selected_file_info() const;

    /** Returns the full path of the selected row, or empty string. */
    std::string selected_path() const;

    /** Activate the current selection: open file or toggle folder expansion. */
    void activate_selection();

    /**
     * Recursively expand or collapse the currently-selected row. When expanding,
     * every descendant directory is expanded as well so the whole subtree is
     * visible in one click. Returns true if anything changed.
     */
    bool toggle_recursive_expand_selected();

private:
    std::set<std::string> m_submodule_paths;
    std::unordered_map<std::string, submodule::SubmoduleInfo> m_metadata_cache;

    std::unique_ptr<ase::gtk::ListView> m_list_view;
    std::unique_ptr<ase::gtk::TreeListModel> m_tree_model;
    std::unique_ptr<ase::gtk::SingleSelection> m_selection;
    std::unique_ptr<ase::gtk::ListItemFactory> m_factory;
};

}  // namespace ase::explorer

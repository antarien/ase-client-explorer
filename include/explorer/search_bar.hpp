#pragma once

/**
 * @file        search_bar.hpp
 * @brief       SearchBar component: toggleable search entry with visible state
 * @description Owns an ase::adp::gtk::SearchEntry plus the visible-state flag. The
 *              window packs entry() into the header bar, wires the adapter's
 *              on_search_changed / on_stop_search directly on the entry, and
 *              calls toggle() centrally from the Ctrl+F and Escape handlers.
 *
 *              Matches Phase 5 behaviour from the single-file explorer: Ctrl+F
 *              toggles visibility, Escape closes + clears the text, empty text
 *              restores the full tree.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <ase/adp/gtk/widget.hpp>

#include <string>

namespace ase::explorer {

class SearchBar {
public:
    SearchBar();

    /** The underlying entry widget - pack into a HeaderBar and wire handlers on it. */
    ase::adp::gtk::SearchEntry& entry() noexcept { return m_entry; }

    /** Show or hide the entry; showing grabs focus, hiding clears the text. */
    void toggle(bool visible);

    /** Returns the current visibility flag. */
    bool is_visible() const noexcept { return m_visible; }

    /** Returns the current filter text. */
    std::string text() const { return m_entry.get_text(); }

private:
    ase::adp::gtk::SearchEntry m_entry;
    bool m_visible = false;
};

}  // namespace ase::explorer

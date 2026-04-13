/**
 * @file        search_bar.cpp
 * @brief       Implementation for search_bar.hpp
 * @description Entry starts hidden with the "Filter files..." placeholder.
 *              toggle() mirrors the single-file explorer semantics: focusing
 *              when opened, clearing when closed.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <explorer/search_bar.hpp>

namespace ase::explorer {

SearchBar::SearchBar() : m_entry(ase::gtk::SearchEntry::create()) {
    m_entry.set_placeholder_text("Filter files...");
    m_entry.set_visible(false);
}

void SearchBar::toggle(bool visible) {
    m_visible = visible;
    m_entry.set_visible(visible);
    if (visible) {
        m_entry.grab_focus();
    } else {
        m_entry.set_text("");
    }
}

}  // namespace ase::explorer

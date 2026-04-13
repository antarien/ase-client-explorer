#pragma once

/**
 * @file        keyboard_shortcuts.hpp
 * @brief       Window-level keyboard shortcut dispatcher
 * @description Owns an ase::gtk::KeyController with all the explorer's global
 *              shortcuts wired to stored slots. The window supplies one slot
 *              per action and calls build() to attach the controller to the
 *              application window.
 *
 *              Shortcuts (1:1 with the single-file explorer):
 *                F5            - refresh tree
 *                Ctrl + ,      - show settings
 *                Ctrl + F      - toggle search
 *                Ctrl + C      - copy absolute path
 *                Ctrl + Shift+C - copy relative path
 *                Return / KP_Enter - activate (open file or toggle folder)
 *                Escape        - close search if open
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <ase/gtk/application.hpp>

#include <sigc++/slot.h>

#include <utility>

namespace ase::explorer {

class KeyboardShortcuts {
public:
    KeyboardShortcuts() = default;

    template <typename Callback>
    void on_refresh(Callback&& cb) {
        m_on_refresh = sigc::slot<void()>([fn = std::forward<Callback>(cb)]() { fn(); });
    }

    template <typename Callback>
    void on_settings(Callback&& cb) {
        m_on_settings = sigc::slot<void()>([fn = std::forward<Callback>(cb)]() { fn(); });
    }

    template <typename Callback>
    void on_toggle_search(Callback&& cb) {
        m_on_toggle_search = sigc::slot<void()>([fn = std::forward<Callback>(cb)]() { fn(); });
    }

    template <typename Callback>
    void on_copy_absolute(Callback&& cb) {
        m_on_copy_absolute = sigc::slot<void()>([fn = std::forward<Callback>(cb)]() { fn(); });
    }

    template <typename Callback>
    void on_copy_relative(Callback&& cb) {
        m_on_copy_relative = sigc::slot<void()>([fn = std::forward<Callback>(cb)]() { fn(); });
    }

    template <typename Callback>
    void on_activate_selection(Callback&& cb) {
        m_on_activate = sigc::slot<void()>([fn = std::forward<Callback>(cb)]() { fn(); });
    }

    template <typename Callback>
    void on_escape_in_search(Callback&& cb) {
        m_on_escape = sigc::slot<void()>([fn = std::forward<Callback>(cb)]() { fn(); });
    }

    /** Query used by the Escape handler: is the search bar currently open? */
    template <typename Predicate>
    void set_search_open_predicate(Predicate&& pred) {
        m_search_open = sigc::slot<bool()>(
            [p = std::forward<Predicate>(pred)]() -> bool { return p(); });
    }

    /** Build the KeyController and attach it to the application window. */
    void build(ase::gtk::ApplicationWindow& window);

private:
    sigc::slot<void()> m_on_refresh;
    sigc::slot<void()> m_on_settings;
    sigc::slot<void()> m_on_toggle_search;
    sigc::slot<void()> m_on_copy_absolute;
    sigc::slot<void()> m_on_copy_relative;
    sigc::slot<void()> m_on_activate;
    sigc::slot<void()> m_on_escape;
    sigc::slot<bool()> m_search_open;
};

}  // namespace ase::explorer

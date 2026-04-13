#pragma once

/**
 * @file        context_menu.hpp
 * @brief       Right-click popover menu with copy / open / reveal actions
 * @description Builds a Gio::Menu + SimpleActionGroup pair and wires a
 *              secondary-button ClickGesture on the list view so the menu
 *              pops up under the cursor. Every action forwards to a stored
 *              sigc::slot that the window supplies in build(); this keeps
 *              the menu stateless and lets the window centralise the actual
 *              file operations (double-click, clipboard, terminal, reveal).
 *
 *              Items: Open · Open With... · Copy Path · Copy Relative Path
 *              · Open in Terminal · Reveal in File Manager.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <ase/gtk/list_view.hpp>

#include <sigc++/slot.h>

#include <utility>

namespace ase::explorer {

class ContextMenu {
public:
    ContextMenu() = default;

    template <typename Callback>
    void on_open(Callback&& cb) {
        m_on_open = sigc::slot<void()>([fn = std::forward<Callback>(cb)]() { fn(); });
    }

    template <typename Callback>
    void on_open_with(Callback&& cb) {
        m_on_open_with = sigc::slot<void()>([fn = std::forward<Callback>(cb)]() { fn(); });
    }

    template <typename Callback>
    void on_copy_path(Callback&& cb) {
        m_on_copy_path = sigc::slot<void()>([fn = std::forward<Callback>(cb)]() { fn(); });
    }

    template <typename Callback>
    void on_copy_rel_path(Callback&& cb) {
        m_on_copy_rel_path = sigc::slot<void()>([fn = std::forward<Callback>(cb)]() { fn(); });
    }

    template <typename Callback>
    void on_open_terminal(Callback&& cb) {
        m_on_open_terminal = sigc::slot<void()>([fn = std::forward<Callback>(cb)]() { fn(); });
    }

    template <typename Callback>
    void on_reveal(Callback&& cb) {
        m_on_reveal = sigc::slot<void()>([fn = std::forward<Callback>(cb)]() { fn(); });
    }

    /** Build the action group + right-click gesture and attach them to list_view. */
    void build(ase::gtk::ListView& list_view);

private:
    sigc::slot<void()> m_on_open;
    sigc::slot<void()> m_on_open_with;
    sigc::slot<void()> m_on_copy_path;
    sigc::slot<void()> m_on_copy_rel_path;
    sigc::slot<void()> m_on_open_terminal;
    sigc::slot<void()> m_on_reveal;
};

}  // namespace ase::explorer

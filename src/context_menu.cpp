/**
 * @file        context_menu.cpp
 * @brief       Implementation for context_menu.hpp
 * @description Creates the Menu model, registers the ActionGroup under the
 *              "explorer" prefix, and attaches a secondary-button ClickGesture
 *              that pops up a PopoverMenu at the cursor coordinates. The
 *              popover is parented to the list view so it is destroyed with
 *              the window.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <explorer/context_menu.hpp>

#include <ase/adp/gtk/menu.hpp>
#include <ase/adp/gtk/gesture.hpp>

#include <memory>

namespace ase::explorer {

namespace {

// Shared state held by the popover's click gesture - the menu model and the
// action group both outlive individual click events.
struct MenuState {
    ase::adp::gtk::Menu menu = ase::adp::gtk::Menu::create();
    ase::adp::gtk::ActionGroup actions = ase::adp::gtk::ActionGroup::create();
};

}  // namespace

void ContextMenu::build(ase::adp::gtk::ListView& list_view) {
    auto state = std::make_shared<MenuState>();

    state->menu.append("Open",                 "explorer.open");
    state->menu.append("Open With...",         "explorer.open-with");
    state->menu.append("Copy Path",            "explorer.copy-path");
    state->menu.append("Copy Relative Path",   "explorer.copy-rel-path");
    state->menu.append("Open in Terminal",     "explorer.open-terminal");
    state->menu.append("Reveal in File Manager","explorer.reveal");
    state->menu.append("Delete...",            "explorer.delete");

    // Each action forwards to its stored slot; copying the slot into the
    // lambda keeps it valid even if the caller re-binds it later.
    auto open          = m_on_open;
    auto open_with     = m_on_open_with;
    auto copy_path     = m_on_copy_path;
    auto copy_rel_path = m_on_copy_rel_path;
    auto open_terminal = m_on_open_terminal;
    auto reveal        = m_on_reveal;
    auto del           = m_on_delete;

    state->actions.add_action("open",          [open]()          { if (open) open(); });
    state->actions.add_action("open-with",     [open_with]()     { if (open_with) open_with(); });
    state->actions.add_action("copy-path",     [copy_path]()     { if (copy_path) copy_path(); });
    state->actions.add_action("copy-rel-path", [copy_rel_path]() { if (copy_rel_path) copy_rel_path(); });
    state->actions.add_action("open-terminal", [open_terminal]() { if (open_terminal) open_terminal(); });
    state->actions.add_action("reveal",        [reveal]()        { if (reveal) reveal(); });
    state->actions.add_action("delete",        [del]()           { if (del) del(); });

    ase::adp::gtk::insert_action_group(list_view, "explorer", state->actions);

    auto right_click = ase::adp::gtk::ClickGesture::create();
    right_click.set_button(ase::adp::gtk::MouseButton::Secondary);
    ase::adp::gtk::ListView* list_ptr = &list_view;
    right_click.on_released([state, list_ptr](int, double x, double y) {
        auto popover = ase::adp::gtk::PopoverMenu::create_from_menu(state->menu);
        popover.set_parent(*list_ptr);
        popover.set_pointing_to(static_cast<int>(x), static_cast<int>(y), 1, 1);
        popover.popup();
    });
    list_view.add_controller(right_click);
}

}  // namespace ase::explorer

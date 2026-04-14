/**
 * @file        keyboard_shortcuts.cpp
 * @brief       Implementation for keyboard_shortcuts.hpp
 * @description Single on_key_pressed handler that dispatches on keyval +
 *              modifiers. Returns true to mark the key as handled so GTK
 *              stops propagation; returns false for unhandled keys so
 *              child widgets (entry, list view) still receive them.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <explorer/keyboard_shortcuts.hpp>

#include <ase/adp/gtk/gesture.hpp>

namespace ase::explorer {

namespace {

// GDK key constants reproduced locally so the file does not depend on gdk/gdk.h
// directly; the adapter already uses numeric keyvals throughout.
constexpr unsigned int KEY_F5         = 0xFFC2;
constexpr unsigned int KEY_Return     = 0xFF0D;
constexpr unsigned int KEY_KP_Enter   = 0xFF8D;
constexpr unsigned int KEY_Escape     = 0xFF1B;
constexpr unsigned int KEY_c          = 0x0063;
constexpr unsigned int KEY_C          = 0x0043;
constexpr unsigned int KEY_f          = 0x0066;
constexpr unsigned int KEY_F          = 0x0046;
constexpr unsigned int KEY_comma      = 0x002C;

}  // namespace

void KeyboardShortcuts::build(ase::adp::gtk::ApplicationWindow& window) {
    auto key = ase::adp::gtk::KeyController::create();

    auto on_refresh       = m_on_refresh;
    auto on_settings      = m_on_settings;
    auto on_toggle_search = m_on_toggle_search;
    auto on_copy_absolute = m_on_copy_absolute;
    auto on_copy_relative = m_on_copy_relative;
    auto on_activate      = m_on_activate;
    auto on_escape        = m_on_escape;
    auto search_open      = m_search_open;

    key.on_key_pressed([=](unsigned int keyval, unsigned int, ase::adp::gtk::Modifier state) -> bool {
        const bool ctrl  = ase::adp::gtk::has(state, ase::adp::gtk::Modifier::Control);
        const bool shift = ase::adp::gtk::has(state, ase::adp::gtk::Modifier::Shift);

        if (keyval == KEY_F5) {
            if (on_refresh) on_refresh();
            return true;
        }
        if (ctrl && keyval == KEY_comma) {
            if (on_settings) on_settings();
            return true;
        }
        if (ctrl && (keyval == KEY_f || keyval == KEY_F)) {
            if (on_toggle_search) on_toggle_search();
            return true;
        }
        if (ctrl && (keyval == KEY_c || keyval == KEY_C)) {
            if (shift) {
                if (on_copy_relative) on_copy_relative();
            } else {
                if (on_copy_absolute) on_copy_absolute();
            }
            return true;
        }
        if (keyval == KEY_Return || keyval == KEY_KP_Enter) {
            if (on_activate) on_activate();
            return true;
        }
        if (keyval == KEY_Escape) {
            if (search_open && search_open()) {
                if (on_escape) on_escape();
                return true;
            }
            return false;
        }
        return false;
    });

    window.add_controller(key);
}

}  // namespace ase::explorer

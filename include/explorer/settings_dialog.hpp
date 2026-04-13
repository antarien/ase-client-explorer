#pragma once

/**
 * @file        settings_dialog.hpp
 * @brief       Adwaita-based preferences window for the explorer
 * @description Builds a standalone AdwWindow containing a ToolbarView,
 *              HeaderBar with a ViewSwitcher, and three preference pages
 *              (General, Appearance, Behavior). The dialog is presented
 *              transient-for the explorer window but is non-modal.
 *
 *              Current preferences:
 *                General    - show hidden files, show .gitignore'd files
 *                Appearance - compact mode
 *                Behavior   - single-click open, live file watching,
 *                             terminal emulator command
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <ase/gtk/application.hpp>

namespace ase::explorer::settings_dialog {

/** Show the preferences window transient for the given parent. */
void show(ase::gtk::ApplicationWindow& parent);

}  // namespace ase::explorer::settings_dialog

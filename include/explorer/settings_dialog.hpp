#pragma once

/**
 * @file        settings_dialog.hpp
 * @brief       Adwaita-based preferences window for the explorer
 * @description Builds a standalone AdwWindow containing a ToolbarView,
 *              HeaderBar with a ViewSwitcher, and a set of preference pages.
 *              The dialog is presented transient-for the explorer window but
 *              is non-modal so the user can keep working while configuring.
 *
 *              Pages:
 *                Display       - hidden files, gitignored files, compact mode
 *                Behavior      - single-click open, live file watch, terminal
 *                Associations  - extension → application mapping (search +
 *                                add/remove rows; persisted to JSON via the
 *                                FileAssociations store)
 *
 *              The dialog is styled to match the shader-tuner aesthetic via
 *              CSS rules in theme.cpp (uppercase letter-spaced tabs, MENU_RED
 *              underline on the active tab, monospace font).
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <ase/adp/gtk/application.hpp>

#include <functional>
#include <string>

namespace ase::explorer {
class FileAssociations;
class ExplorerSettings;
}

namespace ase::explorer::settings_dialog {

/**
 * Show the preferences window transient for the given parent. The
 * FileAssociations store is mutated in-place by the Associations tab and
 * saved on every change. The root_path is scanned for present file
 * extensions so the Associations tab can offer them as a clickable list
 * instead of forcing the user to type extensions by hand.
 *
 * The optional on_close callback fires when the dialog window is destroyed,
 * letting the caller refresh any UI that depends on the now-mutated store
 * (for example the tree view's "extension mapped" indicator dots).
 */
void show(ase::adp::gtk::ApplicationWindow& parent,
          FileAssociations& associations,
          ExplorerSettings& settings,
          const std::string& root_path,
          std::function<void()> on_close = {});

}  // namespace ase::explorer::settings_dialog

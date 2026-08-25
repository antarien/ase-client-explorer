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
 *              DER SCHLIESS-RUECKRUF IST EIN FUNKTIONSZEIGER MIT user_data.
 *              Das ist die Form, die diese Einheit ohnehin ueberall fuehrt —
 *              GTK ruft ausschliesslich so zurueck, und die eigenen Rueckrufe
 *              der Umsetzung machen es genauso — dort steht in jedem Rueckruf
 *              ein `static_cast<ExplorerSettings*>(user_data)` als erste Zeile.
 *              Ein Funktionsobjekt musste hier bisher auf den Haldenspeicher
 *              gelegt und vom Fenster verwaltet werden; mit dem Zeigerpaar
 *              faellt diese Verwaltung ersatzlos weg.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <ase/adp/gtk/application.hpp>

#include <string>

namespace ase::explorer {
class FileAssociations;
class ExplorerSettings;
}

namespace ase::explorer::settings_dialog {

/**
 * Called once when the dialog window is destroyed. `user_data` is handed back
 * unchanged from the show() call, exactly as a GTK signal handler receives it.
 */
using CloseFn = void (*)(void* user_data);

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
 *
 * `on_close_data` must outlive the dialog window. The dialog neither owns nor
 * frees it.
 */
void show(ase::adp::gtk::ApplicationWindow& parent,
          FileAssociations& associations,
          ExplorerSettings& settings,
          const std::string& root_path,
          CloseFn on_close = nullptr,
          void* on_close_data = nullptr);

}  // namespace ase::explorer::settings_dialog

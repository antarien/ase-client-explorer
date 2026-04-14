#pragma once

/**
 * @file        open_with_dialog.hpp
 * @brief       Modal "Open With…" dialog with searchable app list
 * @description Replacement for the old GTK FileLauncher::open_containing_folder
 *              shortcut that the context menu used to take. Presents the user
 *              with a centered modal AdwWindow containing a search entry and
 *              a scrollable list of installed XDG applications. Selecting an
 *              app launches it on the file. An optional checkbox stores the
 *              selection as the user's explicit mapping in FileAssociations
 *              for future direct-opens — but the lookup path NEVER falls back
 *              to a system default if the checkbox stays unchecked.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <ase/adp/gtk/application.hpp>

#include <string>

namespace ase::explorer {

class FileAssociations;

namespace open_with_dialog {

/**
 * Present the dialog. The store is consulted for "always use this" persistence
 * — when the user ticks the checkbox the chosen mapping is written and saved.
 */
void show(ase::adp::gtk::ApplicationWindow& parent,
          const std::string& file_path,
          FileAssociations& store);

}  // namespace open_with_dialog
}  // namespace ase::explorer

#pragma once

/**
 * ASE EXPLORER NATIVE - SETTINGS PAGES
 *
 * @file        settings_page.hpp
 * @brief       The Associations page of the preferences dialog, built outside the dialog file
 * @description settings_dialog.cpp assembles the preferences window from its pages. The
 *              Display and Behavior pages stand in that file; the Associations page stands
 *              in settings_assoc_page.cpp, because it is a working surface of its own (two
 *              filtered lists and a mapping strip over the FileAssociations store). Both
 *              files head their sections with the strip of ase::adp::adw::cockpit.
 *
 * @module      ase-client-explorer
 * @layer       5 (Clients)
 * @category    render/ui/widget
 * @usecase     usecase:101130 - map an extension
 * @created     2026-10-19
 * @modified    2026-10-19
 * @version     00.00.01.00001 [seed]
 *
 * CLIENT NATIVE COMPLIANCE
 *
 * [ ] English only - identifiers, comments, messages
 * [ ] META fields complete, @layer 5 (Section 0)
 * [ ] Headings are bare capital lines, cuts are empty lines (Section 0.1)
 * [ ] Form defined in INST_ASE_LINT.md Section 23
 */

#include <string>

typedef struct _GtkWidget GtkWidget;

namespace ase::explorer {
class FileAssociations;
}

namespace ase::explorer::settings_page {

/**
 * The Associations page: the file extensions found under `root_path` on the left, the
 * installed applications on the right, the active mappings as a chip strip below. A click
 * on an application maps the selected extension in `associations` and saves the store.
 * The page owns its state; it is released with the page widget.
 */
GtkWidget* build_associations_page(FileAssociations& associations,
                                   const std::string& root_path);

}  // namespace ase::explorer::settings_page

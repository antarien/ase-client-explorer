#pragma once

/**
 * ASE EXPLORER NATIVE - ASSOCIATIONS VIEW
 *
 * @file        settings_assoc_view.hpp
 * @brief       The state of the Associations page and the views that draw it
 * @description The Associations page of the preferences dialog keeps one CockpitState for
 *              its lifetime. Two files work on it: settings_assoc_page.cpp assembles the
 *              page and wires the filters and selection callbacks, settings_assoc_view.cpp
 *              draws what the state holds - the extension list, the mapping chips, the hint
 *              line and the rows of the application list. A callback changes the state and
 *              asks the views to draw it anew.
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

#include <explorer/app_catalog.hpp>
#include <explorer/extension_scan.hpp>

#include <ase/containers/vector.hpp>

#include <string>

typedef struct _GtkWidget GtkWidget;

namespace ase::explorer {
class FileAssociations;
}

namespace ase::explorer::settings_page {

/** Everything the Associations page holds between two callbacks. */
struct CockpitState {
    FileAssociations*                                       store = nullptr;
    ase::containers::Vector<AppEntry>                       all_apps;    // app_catalog::all()
    ase::containers::Vector<extension_scan::ExtensionCount> scanned;     // extension_scan::scan()
    GtkWidget*                                              extensions_listbox = nullptr;
    GtkWidget*                                              apps_listbox       = nullptr;
    GtkWidget*                                              ext_search_entry   = nullptr;
    GtkWidget*                                              app_search_entry   = nullptr;
    GtkWidget*                                              chips_box          = nullptr;
    GtkWidget*                                              hint_label         = nullptr;
    std::string                                             ext_search_text;
    std::string                                             app_search_text;
    std::string                                             selected_extension;
    bool                                                    suppress_app_select = false;
};

/** Refill the left list from `scanned`, each row with its mapped application, and keep the selection. */
void rebuild_extensions_listbox(CockpitState* state);

/** Refill the bottom strip with one removable chip per mapping of the store. */
void rebuild_chips_strip(CockpitState* state);

/** Set the hint line to what the selected extension maps to, or what to do next. */
void update_hint(CockpitState* state);

/** One row of the application list: a uniform glyph, the application name and its desktop id. */
GtkWidget* build_app_picker_row(const AppEntry& entry);

}  // namespace ase::explorer::settings_page

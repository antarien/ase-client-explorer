#pragma once

/**
 * @file        folder_picker.hpp
 * @brief       Custom modal folder picker — pure GTK4, no portal dependency
 * @description Minimal in-process folder browser written from scratch with
 *              AdwWindow + GtkListBox + ase-fileio. Replaces the broken
 *              GtkFileDialog/GtkPathBar fallback that fires "gtk_box_remove:
 *              GTK_IS_BOX (box) failed" cascades on Hyprland sessions where
 *              no FileChooser portal backend is eligible.
 *
 *              Supports: navigate into subdirectory by clicking, jump up via
 *              the parent button, type an absolute path into the entry,
 *              confirm via Open, cancel via Cancel/Escape.
 *
 *              DER RUECKRUF IST EIN FUNKTIONSZEIGER MIT user_data und kein
 *              Funktionsobjekt. Das ist die Form, die diese Einheit ohnehin
 *              ueberall fuehrt — GTK ruft ausschliesslich so zurueck, und
 *              settings_dialog.cpp macht es an seinen eigenen Rueckrufen
 *              genauso (`static_cast<ExplorerSettings*>(user_data)` als erste
 *              Zeile). Ein Funktionsobjekt haette hier nur die
 *              Aufgabe gehabt, einen Zeiger einzufangen, den der Aufrufer
 *              danach doch wieder auspackt.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <string>

typedef struct _GtkWindow GtkWindow;

namespace ase::explorer::folder_picker {

/**
 * Called with the absolute path the user accepted. `user_data` is handed back
 * unchanged from the show() call, exactly as a GTK signal handler receives it.
 */
using SelectedFn = void (*)(const std::string& path, void* user_data);

/**
 * Present a modal folder picker as a child of `parent`. Starts at
 * `start_path` (or HOME if empty/non-existent). Calls `on_selected` with
 * the absolute path when the user accepts; not called on cancel.
 *
 * `user_data` must outlive the picker window. The picker neither owns nor
 * frees it.
 */
void show(GtkWindow* parent,
          const std::string& start_path,
          SelectedFn on_selected,
          void* user_data);

}  // namespace ase::explorer::folder_picker

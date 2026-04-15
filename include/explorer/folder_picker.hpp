#pragma once

/**
 * @file        folder_picker.hpp
 * @brief       Custom modal folder picker — pure GTK4, no portal dependency
 * @description Minimal in-process folder browser written from scratch with
 *              AdwWindow + GtkListBox + std::filesystem. Replaces the broken
 *              GtkFileDialog/GtkPathBar fallback that fires "gtk_box_remove:
 *              GTK_IS_BOX (box) failed" cascades on Hyprland sessions where
 *              no FileChooser portal backend is eligible.
 *
 *              Supports: navigate into subdirectory by clicking, jump up via
 *              the parent button, type an absolute path into the entry,
 *              confirm via Open, cancel via Cancel/Escape.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <functional>
#include <string>

typedef struct _GtkWindow GtkWindow;

namespace ase::explorer::folder_picker {

/**
 * Present a modal folder picker as a child of `parent`. Starts at
 * `start_path` (or HOME if empty/non-existent). Calls `on_selected` with
 * the absolute path when the user accepts; not called on cancel.
 */
void show(GtkWindow* parent,
          const std::string& start_path,
          std::function<void(const std::string&)> on_selected);

}  // namespace ase::explorer::folder_picker

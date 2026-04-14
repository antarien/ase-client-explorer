#pragma once

/**
 * @file        clipboard_ops.hpp
 * @brief       Clipboard operations for tree-view paths
 * @description Helpers that copy absolute or repository-relative filesystem
 *              paths to the system clipboard via the adapter's clipboard
 *              helper. Used by the context menu ("Copy Path", "Copy Relative
 *              Path") and the Ctrl+C / Ctrl+Shift+C keyboard shortcuts.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <ase/adp/gtk/list_view.hpp>

#include <string>

namespace ase::explorer::clipboard_ops {

/**
 * Copy a filesystem path to the clipboard. When relative is true and root is
 * non-empty, the copied text is path made relative to root; otherwise the
 * absolute path is copied verbatim. anchor is any widget whose display hosts
 * the clipboard (typically the list view).
 */
void copy_path(ase::adp::gtk::ListView& anchor,
               const std::string& path,
               const std::string& root,
               bool relative);

}  // namespace ase::explorer::clipboard_ops

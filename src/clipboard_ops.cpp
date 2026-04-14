/**
 * @file        clipboard_ops.cpp
 * @brief       Implementation for clipboard_ops.hpp
 * @description Thin wrapper around ase::adp::gtk::copy_to_clipboard with the
 *              relative-path computation delegated to ase::utils::fs.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <explorer/clipboard_ops.hpp>

#include <ase/adp/gtk/io.hpp>
#include <ase/utils/fs.hpp>

namespace ase::explorer::clipboard_ops {

void copy_path(ase::adp::gtk::ListView& anchor,
               const std::string& path,
               const std::string& root,
               bool relative)
{
    if (path.empty()) return;
    std::string text = path;
    if (relative && !root.empty()) {
        text = ase::utils::fs::relative_to(path, root);
    }
    ase::adp::gtk::copy_to_clipboard(anchor, text);
}

}  // namespace ase::explorer::clipboard_ops

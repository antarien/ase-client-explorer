#pragma once

/**
 * @file        extension_scan.hpp
 * @brief       Recursive file-extension census for a given root
 * @description Walks a directory tree and counts how many files exist per
 *              extension. Honours explorer::exclude::should_exclude so the
 *              result matches what the user sees in the tree view (no .git,
 *              no node_modules, no editor caches). Used by the Associations
 *              settings tab to offer "extensions actually present in this
 *              project" instead of forcing the user to type them by hand.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <string>
#include <vector>

namespace ase::explorer::extension_scan {

struct ExtensionCount {
    std::string extension;  ///< lowercase, no leading dot
    int         count = 0;
};

/**
 * Recursively scan root and return a list of unique file extensions sorted
 * by file count descending, then alphabetically. Files without an extension
 * and entries matching exclude::should_exclude are skipped. Hidden dotfiles
 * (whose basename starts with '.') are also skipped to mirror the default
 * tree view behaviour.
 */
std::vector<ExtensionCount> scan(const std::string& root);

}  // namespace ase::explorer::extension_scan

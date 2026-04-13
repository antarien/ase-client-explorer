/**
 * @file        submodule.hpp
 * @brief       Submodule detection and VERSION-file metadata parsing
 * @description Parses a repository's .gitmodules file to enumerate submodule
 *              paths, and reads each submodule's VERSION file to extract the
 *              ASE layer number, status string, and version triple. The tree
 *              view uses this metadata to render status badges per submodule.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#pragma once

#include <cstdint>
#include <set>
#include <string>

namespace ase::explorer::submodule {

/** Metadata parsed from a submodule's VERSION file (all fields optional). */
struct SubmoduleInfo {
    int layer = -1;           ///< L0..L5, or -1 if VERSION file missing/malformed
    std::string status;       ///< stub, poc, init, core, feat, refine, alpha, beta, stable
    std::string version;      ///< e.g. "00.05.23.00273"
};

/** Read and parse the VERSION file inside the given submodule directory. */
SubmoduleInfo parse_version_file(const std::string& dir_path);

/** Enumerate submodule absolute paths by reading root_path/.gitmodules. */
std::set<std::string> parse_gitmodules(const std::string& root_path);

/** Map a status string to its 0xAARRGGBB badge colour. */
uint32_t status_color(const std::string& status);

}  // namespace ase::explorer::submodule

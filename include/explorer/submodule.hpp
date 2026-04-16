/**
 * @file        submodule.hpp
 * @brief       Submodule detection and VERSION metadata parsing
 * @description Parses a repository's .gitmodules file to enumerate submodule
 *              paths, and parses the repository root VERSION file (the SSOT
 *              per WORK_ASE_VERSION_CATALOG.md) to resolve layer/status/version
 *              per submodule. The tree view uses this metadata to render
 *              status badges. Submodule-local VERSION files are 1:1 mirrors
 *              written by `ase version sync` and are NOT read directly —
 *              reading the root ensures the explorer shows the authoritative
 *              state even when a mirror is stale.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>

namespace ase::explorer::submodule {

/** Metadata resolved from the root /VERSION SSOT (all fields optional). */
struct SubmoduleInfo {
    int layer = -1;           ///< L0..L5, or -1 if no entry in root /VERSION
    std::string status;       ///< seed, poc, init, core, feat, refine, alpha, beta, stable
    std::string version;      ///< e.g. "00.05.23.00273"
};

/**
 * Parse `<root_path>/VERSION` once and resolve each submodule in
 * `submodule_paths` to its SubmoduleInfo. The result maps absolute submodule
 * directory paths to the data extracted from the root VERSION sections. The
 * section key per submodule is derived from the basename: dashes are
 * replaced with underscores and the whole thing is uppercased
 * (e.g. `ase-pl-sky` → `ASE_PL_SKY`, `aow-client-web` → `AOW_CLIENT_WEB`).
 * Submodules without an entry are simply omitted.
 */
std::unordered_map<std::string, SubmoduleInfo> parse_root_version(
    const std::string& root_path,
    const std::set<std::string>& submodule_paths);

/** Enumerate submodule absolute paths by reading root_path/.gitmodules. */
std::set<std::string> parse_gitmodules(const std::string& root_path);

/** Map a status string to its 0xAARRGGBB badge colour. */
uint32_t status_color(const std::string& status);

}  // namespace ase::explorer::submodule

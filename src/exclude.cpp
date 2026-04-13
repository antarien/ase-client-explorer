/**
 * @file        exclude.cpp
 * @brief       Implementation for exclude.hpp
 * @description Exact-match set for the well-known filenames plus a prefix check
 *              for cmake-build-* variants. Set lookup is O(log n) which is
 *              dwarfed by filesystem-stat cost on the calling side.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <explorer/exclude.hpp>

#include <set>

namespace ase::explorer::exclude {

namespace {

const std::set<std::string>& excluded_names() {
    static const std::set<std::string> names = {
        "build", "cmake-build-debug", "cmake-build-release", ".cache",
        "node_modules", ".git", ".idea", ".vscode", "__pycache__",
        ".DS_Store", "dist", ".tsbuildinfo",
    };
    return names;
}

}  // namespace

bool should_exclude(const std::string& name) {
    if (excluded_names().count(name) > 0) return true;
    if (name.size() > 12 && name.substr(0, 12) == "cmake-build-") return true;
    return false;
}

}  // namespace ase::explorer::exclude

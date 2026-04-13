/**
 * @file        submodule.cpp
 * @brief       Implementation for submodule.hpp
 * @description VERSION parsing walks each line looking for _LAYER/_STATUS/version
 *              patterns. The gitmodules parser extracts `path = ...` lines and
 *              resolves them against the root. Both helpers go through the
 *              ase::fileio line reader to keep direct fstream usage out.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <explorer/submodule.hpp>

#include <ase/utils/fs.hpp>
#include <ase/fileio/text_reader.hpp>

#include <cctype>
#include <cstdint>
#include <cstdlib>

namespace ase::explorer::submodule {

SubmoduleInfo parse_version_file(const std::string& dir_path) {
    SubmoduleInfo info;
    auto version_path = ase::utils::fs::Path(dir_path) / "VERSION";
    if (!ase::utils::fs::exists(version_path.str())) return info;

    auto lines = ase::fileio::read_lines(version_path.str());
    for (const auto& line : lines) {
        // _LAYER=N
        if (line.find("_LAYER=") != std::string::npos) {
            auto eq = line.find('=');
            if (eq != std::string::npos) {
                auto val = line.substr(eq + 1);
                info.layer = std::atoi(val.c_str());
            }
        }
        // _STATUS=xxx
        if (line.find("_STATUS=") != std::string::npos) {
            auto eq = line.find('=');
            if (eq != std::string::npos) {
                info.status = line.substr(eq + 1);
            }
        }
        // MODULE_NAME=00.05.23.00273 (version number — first non-metadata line)
        if (line.find('=') != std::string::npos && line.find("_NAME") == std::string::npos
            && line.find("_DESC") == std::string::npos && line.find("_LAYER") == std::string::npos
            && line.find("_STATUS") == std::string::npos && line.find("_CREATED") == std::string::npos
            && line.find("_UPDATED") == std::string::npos && info.version.empty()) {
            auto eq = line.find('=');
            auto val = line.substr(eq + 1);
            if (!val.empty() && std::isdigit(static_cast<unsigned char>(val[0]))) {
                info.version = val;
            }
        }
    }
    return info;
}

std::set<std::string> parse_gitmodules(const std::string& root_path) {
    std::set<std::string> submodule_paths;
    auto gitmodules = ase::utils::fs::Path(root_path) / ".gitmodules";
    if (!ase::utils::fs::exists(gitmodules.str())) return submodule_paths;

    auto lines = ase::fileio::read_lines(gitmodules.str());
    for (const auto& raw : lines) {
        auto pos = raw.find("path = ");
        if (pos != std::string::npos) {
            auto rel = raw.substr(pos + 7);
            while (!rel.empty() && (rel.back() == ' ' || rel.back() == '\t')) {
                rel.pop_back();
            }
            auto abs = (ase::utils::fs::Path(root_path) / rel).str();
            submodule_paths.insert(abs);
        }
    }
    return submodule_paths;
}

uint32_t status_color(const std::string& status) {
    if (status == "stub")   return 0xFF4A4A4A;  // grey
    if (status == "poc")    return 0xFF9C8C4A;  // yellow
    if (status == "init")   return 0xFFB8863A;  // orange
    if (status == "core")   return 0xFF5A9CB8;  // blue
    if (status == "feat")   return 0xFF5A9CB8;  // cyan-ish
    if (status == "refine") return 0xFF4A8C6A;  // green
    if (status == "alpha")  return 0xFF7A5A9C;  // purple
    if (status == "beta")   return 0xFF7A5A9C;  // purple
    if (status == "stable") return 0xFF6A9A5A;  // bright green
    return 0xFF5A5A5A;                           // unknown
}

}  // namespace ase::explorer::submodule

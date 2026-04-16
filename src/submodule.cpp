/**
 * @file        submodule.cpp
 * @brief       Implementation for submodule.hpp
 * @description Parses the repository root /VERSION file (SSOT per
 *              WORK_ASE_VERSION_CATALOG.md) into prefix-keyed sections and
 *              resolves each submodule by deriving its section key from the
 *              directory basename (dashes → underscores, uppercase). The
 *              gitmodules parser extracts `path = ...` lines and resolves
 *              them against the root. Both helpers go through the
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

namespace {

// Derive the ASE VERSION section key from a submodule directory's basename.
// `ase-math` → `ASE_MATH`, `ase-pl-sky` → `ASE_PL_SKY`,
// `aow-client-web` → `AOW_CLIENT_WEB`.
std::string key_for_submodule(const std::string& abs_path) {
    auto slash = abs_path.find_last_of('/');
    std::string base = (slash == std::string::npos) ? abs_path : abs_path.substr(slash + 1);
    for (char& c : base) {
        if (c == '-') c = '_';
        else c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return base;
}

bool ends_with(const std::string& s, const char* suffix) {
    const auto n = std::char_traits<char>::length(suffix);
    return s.size() > n && s.compare(s.size() - n, n, suffix) == 0;
}

// A "version line" in the root VERSION SSOT is one whose value has the
// MM.mm.pp.BBBBB shape — starts with a digit and contains a dot. Metric
// lines (ASE_MATH_LOC=2500) start with a digit but have no dot, so this
// discriminator is sufficient to separate the version row from numeric
// metadata rows.
bool looks_like_version_value(const std::string& v) {
    return !v.empty()
        && std::isdigit(static_cast<unsigned char>(v[0])) != 0
        && v.find('.') != std::string::npos;
}

}  // namespace

std::unordered_map<std::string, SubmoduleInfo> parse_root_version(
    const std::string& root_path,
    const std::set<std::string>& submodule_paths) {
    std::unordered_map<std::string, SubmoduleInfo> result;
    auto version_path = ase::utils::fs::Path(root_path) / "VERSION";
    if (!ase::utils::fs::exists(version_path.str())) return result;

    // Phase 1: slurp root /VERSION into a prefix-keyed section map. The
    // root file contains every submodule's authoritative metadata — the
    // per-submodule VERSION files are only 1:1 mirrors distributed by
    // `ase version sync` and must NOT be consulted here (they may be stale
    // or, for a fresh submodule, not yet distributed).
    std::unordered_map<std::string, SubmoduleInfo> by_prefix;
    auto lines = ase::fileio::read_lines(version_path.str());
    for (const auto& raw : lines) {
        if (raw.empty() || raw[0] == '#') continue;
        auto eq = raw.find('=');
        if (eq == std::string::npos || eq == 0) continue;
        std::string key = raw.substr(0, eq);
        std::string val = raw.substr(eq + 1);

        if (ends_with(key, "_LAYER")) {
            auto prefix = key.substr(0, key.size() - 6);
            by_prefix[prefix].layer = std::atoi(val.c_str());
        } else if (ends_with(key, "_STATUS")) {
            auto prefix = key.substr(0, key.size() - 7);
            by_prefix[prefix].status = val;
        } else if (looks_like_version_value(val)) {
            // The key itself is the section prefix (e.g. ASE_PL_SKY=...).
            by_prefix[key].version = val;
        }
        // All other metadata rows (_NAME, _DESC, _COMMITS, _LOC, _TAGS, ...)
        // are intentionally ignored — the explorer's badge only needs layer,
        // status and version.
    }

    // Phase 2: resolve each submodule path to its section by deriving the
    // key from the directory basename. Submodules without an entry in the
    // root catalog are omitted (no badge rendered), matching the previous
    // "missing VERSION file" behaviour.
    for (const auto& abs : submodule_paths) {
        auto it = by_prefix.find(key_for_submodule(abs));
        if (it != by_prefix.end()) result.emplace(abs, it->second);
    }
    return result;
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
    if (status == "seed")   return 0xFF4A4A4A;  // grey
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

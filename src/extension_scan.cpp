/**
 * @file        extension_scan.cpp
 * @brief       Implementation for extension_scan.hpp
 * @description Synchronous std::filesystem walk. Acceptable here because the
 *              scan only runs on Settings dialog open, not on every tree
 *              repaint. Excluded directories are pruned via the existing
 *              explorer::exclude predicate so the count matches the visible
 *              tree.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <explorer/extension_scan.hpp>

#include <explorer/exclude.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <system_error>
#include <unordered_map>

namespace ase::explorer::extension_scan {

namespace fs = std::filesystem;

namespace {

std::string lowercase_extension(const fs::path& p) {
    const std::string name = p.filename().string();
    if (name.empty() || name[0] == '.') return {};   // hidden files & dotfiles
    const auto dot = name.rfind('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 >= name.size()) return {};
    std::string ext = name.substr(dot + 1);
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

}  // namespace

std::vector<ExtensionCount> scan(const std::string& root) {
    std::unordered_map<std::string, int> counts;

    if (root.empty()) return {};
    std::error_code ec;
    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) return {};

    fs::recursive_directory_iterator it(root,
        fs::directory_options::skip_permission_denied, ec);
    fs::recursive_directory_iterator end;

    while (it != end) {
        const fs::directory_entry& entry = *it;
        const std::string name = entry.path().filename().string();

        if (exclude::should_exclude(name)) {
            if (entry.is_directory(ec)) {
                it.disable_recursion_pending();
            }
            it.increment(ec);
            continue;
        }

        if (entry.is_regular_file(ec)) {
            const std::string ext = lowercase_extension(entry.path());
            if (!ext.empty()) counts[ext] += 1;
        }

        it.increment(ec);
    }

    std::vector<ExtensionCount> out;
    out.reserve(counts.size());
    for (const auto& [ext, n] : counts) out.push_back({ext, n});

    std::sort(out.begin(), out.end(),
              [](const ExtensionCount& a, const ExtensionCount& b) {
                  if (a.count != b.count) return a.count > b.count;
                  return a.extension < b.extension;
              });
    return out;
}

}  // namespace ase::explorer::extension_scan

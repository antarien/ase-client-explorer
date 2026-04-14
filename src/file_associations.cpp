/**
 * @file        file_associations.cpp
 * @brief       Implementation for file_associations.hpp
 * @description JSON-on-disk store at ~/.config/ase/explorer/file-associations.json.
 *              Schema is intentionally flat: { ".cpp": "subl.desktop", ... }.
 *              All extensions are normalized to lowercase, no leading dot,
 *              before storage and lookup so callers can pass either form.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <explorer/file_associations.hpp>

#include <ase/json/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>

namespace ase::explorer {

namespace fs = std::filesystem;
using ase::json::Json;

std::string FileAssociations::normalize_extension(const std::string& ext) {
    std::string out;
    out.reserve(ext.size());
    for (char c : ext) {
        if (c == '.' && out.empty()) continue;
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

fs::path FileAssociations::default_store_path() {
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg) {
        return fs::path(xdg) / "ase" / "explorer" / "file-associations.json";
    }
    if (const char* home = std::getenv("HOME"); home && *home) {
        return fs::path(home) / ".config" / "ase" / "explorer" / "file-associations.json";
    }
    return fs::path(".config") / "ase" / "explorer" / "file-associations.json";
}

FileAssociations FileAssociations::load() {
    FileAssociations out;
    out.m_path = default_store_path();

    std::error_code ec;
    if (!fs::exists(out.m_path, ec)) {
        return out;
    }

    std::ifstream in(out.m_path);
    if (!in) return out;

    Json doc = Json::parse(in, /*cb*/ nullptr, /*allow_exceptions*/ false);
    if (!doc.is_object()) return out;

    for (auto it = doc.begin(); it != doc.end(); ++it) {
        if (!it.value().is_string()) continue;
        const std::string ext = normalize_extension(it.key());
        const std::string id  = it.value().get<std::string>();
        if (ext.empty() || id.empty()) continue;
        out.m_map[ext] = id;
    }
    return out;
}

void FileAssociations::save() const {
    std::error_code ec;
    fs::create_directories(m_path.parent_path(), ec);

    Json doc = Json::object();
    for (const auto& [ext, id] : m_map) {
        doc["." + ext] = id;
    }

    std::ofstream out(m_path);
    if (!out) return;
    out << doc.dump(2);
}

std::string FileAssociations::lookup(const std::string& extension) const {
    const std::string key = normalize_extension(extension);
    auto it = m_map.find(key);
    if (it == m_map.end()) return {};
    return it->second;
}

void FileAssociations::set(const std::string& extension, const std::string& desktop_id) {
    const std::string key = normalize_extension(extension);
    if (key.empty() || desktop_id.empty()) return;
    m_map[key] = desktop_id;
}

void FileAssociations::remove(const std::string& extension) {
    m_map.erase(normalize_extension(extension));
}

std::vector<std::pair<std::string, std::string>> FileAssociations::all() const {
    std::vector<std::pair<std::string, std::string>> out;
    out.reserve(m_map.size());
    for (const auto& kv : m_map) out.emplace_back(kv.first, kv.second);
    std::sort(out.begin(), out.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    return out;
}

}  // namespace ase::explorer

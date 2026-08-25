/**
 * @file        file_associations.cpp
 * @brief       Implementation for file_associations.hpp
 * @description JSON-on-disk store at ~/.config/ase/explorer/file-associations.json.
 *              Schema is intentionally flat: { ".cpp": "subl.desktop", ... }.
 *              All extensions are normalized to lowercase, no leading dot,
 *              before storage and lookup so callers can pass either form.
 *
 *              PFAD- UND DATEIZUGRIFF LAUFEN UEBER ase-fileio (L0), Stelle
 *              fuer Stelle:
 *
 *                Pfadverkettung     → fileio::path_join
 *                Existenzpruefung   → fileio::path_exists
 *                Elternverzeichnis  → fileio::parent_of
 *                Verzeichnisbau     → fileio::create_directories
 *                Lesen              → fileio::read_text
 *                Schreiben          → fileio::write_text
 *
 *              DIE EXISTENZFRAGE IST DIE WEITERE, nicht die engere:
 *              path_exists ist fuer JEDEN Eintrag wahr, auch fuer ein
 *              Verzeichnis — genau das tat die abgeloeste Fassung auch. Die
 *              engere Frage heisst file_exists und wuerde das Verhalten hier
 *              stillschweigend aendern.
 *
 *              WARUM HIER KEINE FEHLERMELDUNG STEHT, obwohl Lesen und
 *              Schreiben scheitern koennen: diese Einheit bindet ase::log
 *              nicht (CMakeLists.txt, target_link_libraries). Ein
 *              Lesefehlschlag endet wie vor der Umstellung in einem leeren
 *              Speicher, ein Schreibfehlschlag bleibt wie vorher still. Das
 *              ist eine bewusste Luecke; sie zu schliessen braucht zuerst
 *              eine Log-Kante fuer diesen Client.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <explorer/file_associations.hpp>

#include <ase/fileio/path.hpp>
#include <ase/fileio/text_reader.hpp>
#include <ase/fileio/text_writer.hpp>
#include <ase/json/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

namespace ase::explorer {

using ase::json::Json;

namespace {

/// Append "ase/explorer/<file_name>" to a config root. Kept in one place so the
/// three roots below cannot drift apart.
std::string store_path_under(const std::string& config_root, const std::string& file_name) {
    std::string out = fileio::path_join(config_root, "ase");
    out             = fileio::path_join(out, "explorer");
    return fileio::path_join(out, file_name);
}

}  // anonymous namespace

std::string FileAssociations::normalize_extension(const std::string& ext) {
    std::string out;
    out.reserve(ext.size());
    for (char c : ext) {
        if (c == '.' && out.empty()) continue;
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

std::string FileAssociations::default_store_path() {
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg) {
        return store_path_under(xdg, "file-associations.json");
    }
    if (const char* home = std::getenv("HOME"); home && *home) {
        return store_path_under(fileio::path_join(home, ".config"), "file-associations.json");
    }
    return store_path_under(".config", "file-associations.json");
}

FileAssociations FileAssociations::load() {
    FileAssociations out;
    out.m_path = default_store_path();

    if (!fileio::path_exists(out.m_path)) {
        return out;
    }

    // An empty result covers both "unreadable" and "empty file"; both end in an empty
    // store, exactly as the stream-based version did.
    const std::string text = fileio::read_text(out.m_path);
    if (text.empty()) return out;

    Json doc = Json::parse(text, /*cb*/ nullptr, /*allow_exceptions*/ false);
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
    // Already-exists counts as success here, which is what this call site needs: the
    // directory usually survives from the previous run.
    fileio::create_directories(fileio::parent_of(m_path));

    Json doc = Json::object();
    for (const auto& [ext, id] : m_map) {
        doc["." + ext] = id;
    }

    fileio::write_text(m_path, doc.dump(2));
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

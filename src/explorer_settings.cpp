/**
 * @file        explorer_settings.cpp
 * @brief       Implementation for explorer_settings.hpp
 * @description Same JSON-on-disk pattern as FileAssociations. Schema is
 *              flat:  { "breadcrumb_max_segments": 5, "default_root": "..." }
 *              Missing keys fall back to the default member values, so
 *              older settings files keep working when new fields are added.
 *
 *              PFAD- UND DATEIZUGRIFF LAUFEN UEBER ase-fileio (L0). Die
 *              Zuordnung Stelle fuer Stelle, damit der Naechste nicht raten
 *              muss, was hier vor dem 2026-08-22 stand:
 *
 *                Pfadverkettung     → fileio::path_join
 *                Existenzpruefung   → fileio::path_exists
 *                Elternverzeichnis  → fileio::parent_of
 *                Verzeichnisbau     → fileio::create_directories
 *                Lesen              → fileio::read_text
 *                Schreiben          → fileio::write_text
 *                Begrenzen          → ase::math::clamp
 *
 *              DIE EXISTENZFRAGE IST DIE WEITERE, nicht die engere, und das
 *              ist Absicht: path_exists ist fuer JEDEN Eintrag wahr, auch
 *              fuer ein Verzeichnis. Genau das tat die abgeloeste Fassung
 *              auch. Die engere Frage — liegt hier eine regulaere Datei —
 *              heisst file_exists und wuerde hier das Verhalten aendern.
 *
 *              DIE FRUEHERE LUECKE IST GESCHLOSSEN, in der Reihenfolge, die
 *              hier stand: zuerst die Log-Kante fuer diesen Client, dann die
 *              Zeilen. Ausgeloest hat es der Vorgang um den libcgraph-Soname-
 *              Sprung, bei dem ein Doppelklick monatelang nichts tat und
 *              keine Einheit dieses Clients es melden konnte.
 *
 *              DER SCHREIBFEHLSCHLAG MELDET, DER LESEFEHLSCHLAG NICHT, und
 *              das ist kein Rest der alten Luecke: ein unlesbarer Bestand
 *              endet in den Vorgabewerten und ist von einem nie angelegten
 *              Speicher nicht unterscheidbar, weil read_text beide als leere
 *              Zeichenkette zurueckgibt. Ein fehlgeschlagener Schrieb
 *              dagegen verwirft eine Einstellung, die gesetzt wurde.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <explorer/explorer_settings.hpp>

#include <ase/fileio/path.hpp>
#include <ase/fileio/text_reader.hpp>
#include <ase/fileio/text_writer.hpp>
#include <ase/json/json.hpp>
#include <ase/log/log.hpp>
#include <ase/math/scalar.hpp>

#include <cstdlib>
#include <string>

namespace ase::explorer {

using ase::json::Json;

namespace {

constexpr const char* kLogSystem = "ExplorerSettings";

/// Append the fixed tail "ase/explorer/settings.json" to a config root.
/// Kept in one place so the three roots below cannot drift apart.
std::string store_path_under(const std::string& config_root) {
    std::string out = fileio::path_join(config_root, "ase");
    out             = fileio::path_join(out, "explorer");
    return fileio::path_join(out, "settings.json");
}

}  // anonymous namespace

std::string ExplorerSettings::default_store_path() {
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg) {
        return store_path_under(xdg);
    }
    if (const char* home = std::getenv("HOME"); home && *home) {
        return store_path_under(fileio::path_join(home, ".config"));
    }
    return store_path_under(".config");
}

ExplorerSettings ExplorerSettings::load() {
    ExplorerSettings out;
    out.m_path = default_store_path();

    if (!fileio::path_exists(out.m_path)) return out;

    // An empty result covers both "unreadable" and "empty file"; both end in the
    // defaults, exactly as the stream-based version did.
    const std::string text = fileio::read_text(out.m_path);
    if (text.empty()) return out;

    Json doc = Json::parse(text, /*cb*/ nullptr, /*allow_exceptions*/ false);
    if (!doc.is_object()) return out;

    if (auto it = doc.find("breadcrumb_max_segments"); it != doc.end() && it->is_number_integer()) {
        out.m_breadcrumb_max = it->get<int>();
    }
    if (auto it = doc.find("default_root"); it != doc.end() && it->is_string()) {
        const std::string s = it->get<std::string>();
        if (!s.empty()) out.m_default_root = s;
    }
    return out;
}

void ExplorerSettings::save() const {
    // Already-exists counts as success here, which is what this call site needs: the
    // directory usually survives from the previous run.
    if (!fileio::create_directories(fileio::parent_of(m_path))) {
        ase::log::error(ase::log::ERR::CAT::HOST_OP_FAILED, kLogSystem,
                        "settings_dir_create", m_path.c_str());
        return;
    }

    Json doc = Json::object();
    doc["breadcrumb_max_segments"] = m_breadcrumb_max;
    doc["default_root"]            = m_default_root;

    // Der Fehlschlag faellt beim naechsten Start auf, nicht hier: die Vorgabewerte sehen
    // aus wie eine nie gesetzte Einstellung, nicht wie eine verlorene.
    if (!fileio::write_text(m_path, doc.dump(2))) {
        ase::log::error(ase::log::ERR::CAT::HOST_OP_FAILED, kLogSystem,
                        "settings_write", m_path.c_str());
    }
}

void ExplorerSettings::set_breadcrumb_max_segments(int n) noexcept {
    m_breadcrumb_max = ase::math::clamp(n, 3, 20);
}

void ExplorerSettings::set_default_root(const std::string& path) {
    if (path.empty()) return;
    m_default_root = path;
}

}  // namespace ase::explorer

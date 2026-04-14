/**
 * @file        explorer_settings.cpp
 * @brief       Implementation for explorer_settings.hpp
 * @description Same JSON-on-disk pattern as FileAssociations. Schema is
 *              flat:  { "breadcrumb_max_segments": 5 }
 *              Missing keys fall back to the default member values, so
 *              older settings files keep working when new fields are added.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <explorer/explorer_settings.hpp>

#include <ase/json/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <fstream>

namespace ase::explorer {

namespace fs = std::filesystem;
using ase::json::Json;

fs::path ExplorerSettings::default_store_path() {
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg) {
        return fs::path(xdg) / "ase" / "explorer" / "settings.json";
    }
    if (const char* home = std::getenv("HOME"); home && *home) {
        return fs::path(home) / ".config" / "ase" / "explorer" / "settings.json";
    }
    return fs::path(".config") / "ase" / "explorer" / "settings.json";
}

ExplorerSettings ExplorerSettings::load() {
    ExplorerSettings out;
    out.m_path = default_store_path();

    std::error_code ec;
    if (!fs::exists(out.m_path, ec)) return out;

    std::ifstream in(out.m_path);
    if (!in) return out;

    Json doc = Json::parse(in, /*cb*/ nullptr, /*allow_exceptions*/ false);
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
    std::error_code ec;
    fs::create_directories(m_path.parent_path(), ec);

    Json doc = Json::object();
    doc["breadcrumb_max_segments"] = m_breadcrumb_max;
    doc["default_root"]            = m_default_root;

    std::ofstream out(m_path);
    if (!out) return;
    out << doc.dump(2);
}

void ExplorerSettings::set_breadcrumb_max_segments(int n) noexcept {
    m_breadcrumb_max = std::clamp(n, 3, 20);
}

void ExplorerSettings::set_default_root(const std::string& path) {
    if (path.empty()) return;
    m_default_root = path;
}

}  // namespace ase::explorer

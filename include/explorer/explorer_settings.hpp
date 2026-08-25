#pragma once

/**
 * @file        explorer_settings.hpp
 * @brief       Per-user explorer preferences persisted as JSON
 * @description Companion to FileAssociations: a generic JSON store under
 *              ~/.config/ase/explorer/settings.json holding all explorer
 *              prefs that are NOT extension associations (max breadcrumb
 *              segments, future toggles for hidden files / single-click /
 *              terminal emulator, etc.).
 *
 *              DER PFAD IST EINE ZEICHENKETTE, kein Pfadobjekt — und das ist
 *              der einzige sichtbare Unterschied zur Fassung vor dem
 *              2026-08-22. Die Pfadbibliothek der Standardbibliothek ist
 *              baumweit gesperrt; die Ersatzform liegt in ase-fileio (L0,
 *              aus jeder Schicht erreichbar) und rechnet auf Zeichenketten:
 *              path_join, parent_of, path_exists, create_directories
 *              (foundation/ase-fileio/include/ase/fileio/path.hpp) sowie
 *              read_text und write_text (text_reader.hpp, text_writer.hpp).
 *
 *              WARUM path() BLEIBT, obwohl es am 2026-08-22 keinen einzigen
 *              fremden Aufrufer hatte — gemessen ueber das ganze Modul, die
 *              drei .path()-Treffer in extension_scan.cpp und
 *              folder_picker.cpp gehoeren zu einem Verzeichniseintrag und
 *              nicht zu dieser Klasse: der Wert ist die einzige Stelle, an
 *              der ein Fehlerfall spaeter benennen kann, WELCHE Datei nicht
 *              gelesen oder nicht geschrieben wurde. Ihn wegzunehmen waere
 *              ein Funktionsverlust ohne Gegenwert, und der Zaehler faellt
 *              dadurch um keinen einzigen Punkt.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <string>

namespace ase::explorer {

class ExplorerSettings {
public:
    static ExplorerSettings load();
    void save() const;

    int  breadcrumb_max_segments() const noexcept { return m_breadcrumb_max; }
    void set_breadcrumb_max_segments(int n) noexcept;

    const std::string& default_root() const noexcept { return m_default_root; }
    void set_default_root(const std::string& path);

    /// Absolute location this instance was loaded from and will save back to.
    const std::string& path() const noexcept { return m_path; }

private:
    static std::string default_store_path();

    int         m_breadcrumb_max = 5;
    std::string m_default_root   = "/mnt/code/SRC/GITHUB/ase";
    std::string m_path;
};

}  // namespace ase::explorer

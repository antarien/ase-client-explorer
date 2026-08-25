/**
 * @file        extension_scan.cpp
 * @brief       Implementation for extension_scan.hpp
 * @description Synchronous walk over ase-fileio (L0). Acceptable here because the
 *              scan only runs on Settings dialog open, not on every tree
 *              repaint. Excluded directories are pruned via the existing
 *              explorer::exclude predicate so the count matches the visible
 *              tree.
 *
 *              DIE WANDERUNG IST VON HAND GESCHRIEBEN, weil ase-fileio nur eine
 *              EBENE liefert (list_dir) und der rekursive Wanderer der
 *              Pfadbibliothek baumweit gesperrt ist. Statt einer Rekursion
 *              laeuft eine Arbeitsliste: kein Stapelrisiko bei tiefen Baeumen,
 *              und der Abstieg ist an genau einer Stelle entschieden.
 *
 *              DREI GRENZEN DER ABGELOESTEN FASSUNG WERDEN AUSDRUECKLICH
 *              MITGENOMMEN — sie sind der Grund, warum hier mehr steht als ein
 *              Namenstausch:
 *
 *              1. KEIN ABSTIEG IN VERZEICHNIS-LINKS. Der Wanderer der
 *                 Pfadbibliothek tut das per Vorgabe nicht, und der Grund ist
 *                 kein Geschmack: zeigt a/b auf a, laeuft ein folgender
 *                 Abstieg unendlich. `is_dir` allein reicht dafuer nicht — es
 *                 stammt aus stat() und ist fuer einen Link auf ein
 *                 Verzeichnis wahr. Die Marke, die hier entscheidet, ist
 *                 `is_symlink` (directory.hpp).
 *              2. KEINE STILLE KAPPUNG. list_dir schreibt in einen Puffer
 *                 fester Groesse und meldet Ueberlauf, indem es genau die
 *                 Kapazitaet zurueckgibt. Wer das nicht auswertet, zaehlt in
 *                 einem grossen Ordner zu wenig Dateien und sieht es nie. Die
 *                 Schleife unten verdoppelt und fragt erneut.
 *              3. UNLESBARES WIRD UEBERSPRUNGEN, nicht abgebrochen. list_dir
 *                 liefert fuer ein Verzeichnis ohne Leserecht 0 — dasselbe
 *                 Verhalten wie skip_permission_denied vorher.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <explorer/extension_scan.hpp>

#include <explorer/exclude.hpp>

#include <ase/fileio/directory.hpp>
#include <ase/fileio/path.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace ase::explorer::extension_scan {

namespace {

/// Entries per list_dir call before the buffer is grown. Sized for an ordinary source
/// directory; the growth loop covers everything above it without losing an entry.
constexpr uint32_t INITIAL_DIR_CAPACITY = 128u;

/// Lowercase extension of a bare entry name, empty when there is none.
/// Hidden entries (leading dot) return empty and are therefore never counted, which
/// mirrors the default tree view.
std::string lowercase_extension(const std::string& name) {
    if (name.empty() || name[0] == '.') return {};   // hidden files & dotfiles
    const auto dot = name.rfind('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 >= name.size()) return {};
    std::string ext = name.substr(dot + 1);
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

}  // namespace

ase::containers::Vector<ExtensionCount> scan(const std::string& root) {
    std::unordered_map<std::string, int> counts;

    if (root.empty()) return {};
    if (!fileio::path_exists(root) || !fileio::is_directory(root)) return {};

    ase::containers::Vector<std::string> pending;
    pending.push_back(root);

    ase::containers::Vector<fileio::DirEntry> buffer(INITIAL_DIR_CAPACITY);

    while (!pending.empty()) {
        const std::string dir = pending.back();
        pending.pop_back();

        // A full buffer is reported as "written == capacity", never as an error, so the
        // only way to tell "exactly full" from "truncated" is to ask again with more room.
        // One extra listing in the exact-fit case is the price for never miscounting.
        uint32_t found = 0u;
        for (;;) {
            const uint32_t capacity = static_cast<uint32_t>(buffer.size());
            found = fileio::list_dir(dir.c_str(), static_cast<uint32_t>(dir.size()),
                                     buffer.data(), capacity);
            if (found < capacity) break;
            buffer.resize(buffer.size() * 2u);
        }

        for (uint32_t i = 0u; i < found; ++i) {
            const fileio::DirEntry& entry = buffer[i];
            const std::string       name(entry.name);

            if (exclude::should_exclude(name)) continue;

            if (entry.is_dir) {
                // See limit 1 in the header: a link to a directory is a directory to
                // stat() and a cycle to a walker.
                if (!entry.is_symlink) pending.push_back(fileio::path_join(dir, name));
                continue;
            }

            if (!entry.is_regular) continue;

            const std::string ext = lowercase_extension(name);
            if (!ext.empty()) counts[ext] += 1;
        }
    }

    ase::containers::Vector<ExtensionCount> out;
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

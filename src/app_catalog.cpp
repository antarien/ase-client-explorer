/**
 * @file        app_catalog.cpp
 * @brief       Implementation for app_catalog.hpp
 * @description Pure Gio bridging code: GAppInfo lists are converted into
 *              ASE-native AppEntry vectors so the rest of the client never
 *              touches GObject lifetime concerns. All g_object_unref happens
 *              here.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <explorer/app_catalog.hpp>

#include <ase/containers/vector.hpp>
#include <ase/fileio/directory.hpp>
#include <ase/fileio/path.hpp>
#include <ase/fileio/text_reader.hpp>
#include <ase/log/log.hpp>
#include <ase/utils/fs.hpp>

#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
#include <glib.h>

#include <unistd.h>  // readlink fuer /proc/self/exe

#include <algorithm>
#include <string>

namespace ase::explorer::app_catalog {

namespace {

constexpr const char* kLogSystem = "ExplorerAppCatalog";

/// Resolve the executable this process runs from, or an empty string.
///
/// /proc/self/exe is the EIGENE Adresse, nicht argv[0]: letzteres traegt, was der Aufrufer
/// hineinschrieb, und bei einem Start ueber eine .desktop ist das der bloße Name.
std::string own_executable() {
    char buf[ase::fileio::DIR_PATH_MAX] = {};
    const ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return {};
    buf[n] = '\0';
    return std::string(buf);
}

/// Find `binary` in a sibling source-tree build, or return an empty string.
///
/// WARUM DAS UEBERHAUPT GEBRAUCHT WIRD: eine .desktop nennt ihr Programm ohne Pfad
/// („Exec=ase-viewer %F"), also entscheidet PATH — und dort gewinnt das Systempraefix. Wer
/// den Explorer aus dem Quellbaum startet und daneben den Betrachter gebaut hat, bekommt
/// trotzdem den INSTALLIERTEN, und der kann beliebig alt sein. Gemessen hat genau das einen
/// Doppelklick monatelang ins Leere laufen lassen, waehrend der frische Bau daneben lag.
///
/// DER NAME WIRD GESUCHT, NICHT GEBILDET. Aus `ase-viewer` auf `ase-client-viewer` zu
/// schliessen waere eine erfundene Abbildung; stattdessen werden die Geschwister unter
/// clients/ aufgezaehlt und in jedem build/bin nach dem Binaer gefragt.
///
/// Die Wurzel kommt aus der EIGENEN Adresse: liegt sie unter <root>/clients/<x>/build/bin,
/// ist <root> vier Ebenen darueber. Laeuft dieser Prozess nicht aus einem Quellbaum — etwa
/// als installiertes Paket —, trifft die Bedingung nicht und es bleibt bei der .desktop.
/// `self` ist ein PARAMETER, damit diese Entscheidung pruefbar ist.
///
/// Mit festem /proc/self/exe liesse sich die Regel nur im laufenden Explorer beobachten, also
/// erst nach einem Bau und nur von Hand. So fuehrt eine Sonde beide Faelle vor: den Pfad
/// eines Quellbaums, der den Geschwisterbau finden MUSS, und den eines installierten
/// Programms, der NICHTS finden darf. Eine Zusicherung, die nur im Betrieb sichtbar ist, ist
/// keine.
std::string sibling_build(const std::string& binary, const std::string& self_path) {
    if (binary.empty()) return {};
    const std::string self = self_path.empty() ? own_executable() : self_path;
    if (self.empty()) return {};

    const std::string bin_dir   = fileio::parent_of(self);             // …/build/bin
    const std::string build_dir = fileio::parent_of(bin_dir);          // …/build
    const std::string client    = fileio::parent_of(build_dir);        // …/clients/<x>
    const std::string clients   = fileio::parent_of(client);           // …/clients
    if (bin_dir.empty() || build_dir.empty() || client.empty() || clients.empty()) return {};

    // Die Form muss stimmen, sonst ist dies kein Quellbaum-Start.
    if (ase::utils::fs::filename_of(bin_dir) != "bin") return {};
    if (ase::utils::fs::filename_of(build_dir) != "build") return {};
    if (ase::utils::fs::filename_of(clients) != "clients") return {};

    ase::fileio::DirEntry entries[64];
    const uint32_t count = ase::fileio::list_dir(
        clients.c_str(), static_cast<uint32_t>(clients.size()), entries, 64u);

    for (uint32_t i = 0; i < count; ++i) {
        if (!entries[i].is_dir) continue;
        std::string cand = fileio::path_join(clients, entries[i].name);
        cand = fileio::path_join(cand, "build");
        cand = fileio::path_join(cand, "bin");
        cand = fileio::path_join(cand, binary);
        if (fileio::file_exists(cand)) return cand;
    }
    return {};
}

AppEntry to_entry(GAppInfo* info) {
    AppEntry e;
    if (!info) return e;
    if (const char* id = g_app_info_get_id(info))         e.desktop_id = id;
    if (const char* nm = g_app_info_get_display_name(info)) e.name = nm;
    if (const char* ex = g_app_info_get_commandline(info))  e.exec = ex;
    if (GIcon* icon = g_app_info_get_icon(info)) {
        if (G_IS_THEMED_ICON(icon)) {
            const gchar* const* names = g_themed_icon_get_names(G_THEMED_ICON(icon));
            if (names && names[0]) e.icon_name = names[0];
        }
    }
    return e;
}

ase::containers::Vector<AppEntry> drain_glist(GList* list) {
    ase::containers::Vector<AppEntry> out;
    for (GList* node = list; node != nullptr; node = node->next) {
        auto* info = static_cast<GAppInfo*>(node->data);
        if (!info) continue;
        if (!g_app_info_should_show(info)) continue;
        AppEntry e = to_entry(info);
        if (!e.desktop_id.empty() && !e.name.empty()) out.push_back(std::move(e));
    }
    g_list_free_full(list, g_object_unref);

    std::sort(out.begin(), out.end(),
              [](const AppEntry& a, const AppEntry& b) { return a.name < b.name; });
    return out;
}

}  // namespace

ase::containers::Vector<AppEntry> all() {
    return drain_glist(g_app_info_get_all());
}

ase::containers::Vector<AppEntry> for_mime_type(const std::string& mime) {
    if (mime.empty()) return {};
    return drain_glist(g_app_info_get_all_for_type(mime.c_str()));
}

std::string mime_type_for_path(const std::string& path) {
    if (path.empty()) return {};
    GFile* file = g_file_new_for_path(path.c_str());
    if (!file) return {};
    GError* err = nullptr;
    GFileInfo* info = g_file_query_info(
        file,
        G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
        G_FILE_QUERY_INFO_NONE,
        nullptr, &err);
    std::string mime;
    if (info) {
        if (const char* ct = g_file_info_get_content_type(info)) mime = ct;
        g_object_unref(info);
    }
    if (err) g_error_free(err);
    g_object_unref(file);
    return mime;
}

AppEntry find_by_id(const std::string& desktop_id) {
    if (desktop_id.empty()) return {};
    GDesktopAppInfo* d = g_desktop_app_info_new(desktop_id.c_str());
    if (!d) return {};
    AppEntry e = to_entry(G_APP_INFO(d));
    g_object_unref(d);
    return e;
}

/// Start the application named by `desktop_id` on `file_path`.
///
/// WAS EIN TRUE VON HIER BELEGT UND WAS NICHT: g_app_info_launch kehrt mit TRUE zurueck,
/// sobald der Prozess ANGELEGT ist. Alles danach liegt ausserhalb seiner Zusage — ein Kind,
/// das am dynamischen Linker stirbt, ist an dieser Schnittstelle von einem laufenden
/// Programm nicht zu unterscheiden. Diese Grenze ist gemessen und nicht vermutet: ein
/// ase-viewer, dessen libcgraph-Soname von 8 auf 10 gesprungen war, startete monatelang
/// nicht, und der Startweg meldete jedes Mal Erfolg.
///
/// WER DEN RUECKGABEWERT VERWIRFT, MACHT DARAUS DAS, WAS ER NIE BEHAUPTET HAT. Deshalb
/// meldet diese Einheit beide Fehlerfaelle selbst, statt sie dem Aufrufer zu ueberlassen:
/// die Stelle hier kennt den GError, der Aufrufer nur ein bool.
bool launch(const std::string& desktop_id, const std::string& file_path) {
    if (desktop_id.empty() || file_path.empty()) {
        ase::log::error(ase::log::ERR::CAT::INPUT_REJECTED, kLogSystem,
                        "app_launch", "desktop id or file path empty");
        return false;
    }
    GDesktopAppInfo* d = g_desktop_app_info_new(desktop_id.c_str());
    if (!d) {
        // UNKNOWN_ID: die Kennung steht in keiner Registry. Das trifft eine Verknuepfung auf
        // eine .desktop, die deinstalliert oder umbenannt wurde — der Speicher haelt sie
        // weiter, und ohne diese Zeile endete der Doppelklick lautlos.
        ase::log::error(ase::log::ERR::CAT::UNKNOWN_ID, kLogSystem,
                        "app_launch_desktop_lookup", desktop_id.c_str());
        return false;
    }

    GFile* file = g_file_new_for_path(file_path.c_str());
    GList* files = g_list_append(nullptr, file);

    // EIN QUELLBAUM STARTET SEINE GESCHWISTER, NICHT DAS SYSTEMPRAEFIX.
    //
    // Laeuft dieser Explorer selbst aus einem build/bin und liegt das Programm der .desktop
    // daneben gebaut, gewinnt der frische Bau. Ohne diesen Zug entscheidet PATH, und dort
    // steht das installierte Paket vorn: wer gerade beide Werkzeuge gebaut hat, bekaeme
    // dennoch den alten Stand und sieht dem Doppelklick nicht an, welchen er startete.
    // Gemessen hat genau das einen Doppelklick monatelang ins Leere laufen lassen, waehrend
    // der frische Bau daneben lag.
    //
    // DIE .desktop BLEIBT DIE SSOT DER ZUORDNUNG — sie sagt, WELCHES Programm die Endung
    // oeffnet. Hier wird allein entschieden, WELCHES EXEMPLAR davon laeuft. Gebaut wird der
    // Start deshalb als GAppInfo aus der lokalen Befehlszeile und nicht als eigener Prozess:
    // dieselbe Schnittstelle, dieselbe Uebergabe der Dateien, nur ein anderes Programm.
    GAppInfo* local_app = nullptr;
    if (const char* exe = g_desktop_app_info_get_string(d, "Exec")) {
        gchar** parts = g_strsplit(exe, " ", 2);
        const std::string wanted = (parts != nullptr && parts[0] != nullptr)
            ? std::string(parts[0]) : std::string();
        g_strfreev(parts);
        const std::string local = sibling_build(wanted, std::string());
        if (!local.empty()) {
            GError* mk_err = nullptr;
            local_app = g_app_info_create_from_commandline(
                local.c_str(), nullptr, G_APP_INFO_CREATE_NONE, &mk_err);
            if (local_app == nullptr) {
                // Der Geschwisterbau liegt da und ist nicht benutzbar: ein Befund, kein
                // Grund zum Schweigen. Danach gilt wieder die .desktop.
                ase::log::warn(ase::log::WRN::CAT::HOST_OP_FAILED, kLogSystem,
                               "app_launch_sibling",
                               mk_err != nullptr && mk_err->message != nullptr
                                   ? mk_err->message : local.c_str());
            } else {
                ase::log::debug(std::string("[") + kLogSystem
                                + "] sibling build wins over PATH: " + local);
            }
            if (mk_err != nullptr) g_error_free(mk_err);
        }
    }

    GError* err = nullptr;
    gboolean ok = g_app_info_launch(
        local_app != nullptr ? local_app : G_APP_INFO(d), files, nullptr, &err);
    if (local_app != nullptr) g_object_unref(local_app);

    if (ok != TRUE) {
        // HOST_OP_FAILED: die Operation lief und ist gescheitert. Der GError traegt den Text
        // des Betriebssystems und wird GELESEN, bevor er freigegeben wird.
        ase::log::error(ase::log::ERR::CAT::HOST_OP_FAILED, kLogSystem,
                        "app_launch", err && err->message ? err->message : "no error text");
    }

    g_list_free(files);
    g_object_unref(file);
    g_object_unref(d);
    if (err) g_error_free(err);
    return ok == TRUE;
}

}  // namespace ase::explorer::app_catalog

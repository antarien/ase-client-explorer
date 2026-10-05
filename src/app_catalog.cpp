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
#include <ase/log/log.hpp>

#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
#include <glib.h>

#include <algorithm>

namespace ase::explorer::app_catalog {

namespace {

constexpr const char* kLogSystem = "ExplorerAppCatalog";

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

    GError* err = nullptr;
    gboolean ok = g_app_info_launch(G_APP_INFO(d), files, nullptr, &err);

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

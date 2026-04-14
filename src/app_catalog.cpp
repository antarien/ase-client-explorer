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

#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
#include <glib.h>

#include <algorithm>

namespace ase::explorer::app_catalog {

namespace {

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

std::vector<AppEntry> drain_glist(GList* list) {
    std::vector<AppEntry> out;
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

std::vector<AppEntry> all() {
    return drain_glist(g_app_info_get_all());
}

std::vector<AppEntry> for_mime_type(const std::string& mime) {
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

bool launch(const std::string& desktop_id, const std::string& file_path) {
    if (desktop_id.empty() || file_path.empty()) return false;
    GDesktopAppInfo* d = g_desktop_app_info_new(desktop_id.c_str());
    if (!d) return false;

    GFile* file = g_file_new_for_path(file_path.c_str());
    GList* files = g_list_append(nullptr, file);

    GError* err = nullptr;
    gboolean ok = g_app_info_launch(G_APP_INFO(d), files, nullptr, &err);

    g_list_free(files);
    g_object_unref(file);
    g_object_unref(d);
    if (err) g_error_free(err);
    return ok == TRUE;
}

}  // namespace ase::explorer::app_catalog

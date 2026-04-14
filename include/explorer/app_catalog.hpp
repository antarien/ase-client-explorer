#pragma once

/**
 * @file        app_catalog.hpp
 * @brief       XDG / Gio application catalog snapshot
 * @description Thin Gio AppInfo wrapper that enumerates the desktop
 *              applications installed on the system. Reads the standard
 *              XDG locations (/usr/share/applications, ~/.local/share/applications,
 *              flatpak exports) via g_app_info_get_all(), so the result
 *              matches what other XDG launchers (Hyprlaunch, fuzzel, etc.)
 *              show. Pure data — no UI.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <string>
#include <vector>

namespace ase::explorer {

struct AppEntry {
    std::string desktop_id;   ///< e.g. "sublime_text.desktop"
    std::string name;         ///< human-readable display name
    std::string exec;         ///< command line, %f / %F placeholders may remain
    std::string icon_name;    ///< themed icon name (may be empty)
};

namespace app_catalog {

/** Snapshot every installed XDG application sorted by display name. */
std::vector<AppEntry> all();

/** Snapshot applications that declared the given MIME type as a handler. */
std::vector<AppEntry> for_mime_type(const std::string& mime);

/** Probe the MIME type of a file path; empty if it cannot be determined. */
std::string mime_type_for_path(const std::string& path);

/** Look up an entry by its desktop id; empty AppEntry::desktop_id if missing. */
AppEntry find_by_id(const std::string& desktop_id);

/** Launch the desktop application on the given file path. Returns false on failure. */
bool launch(const std::string& desktop_id, const std::string& file_path);

}  // namespace app_catalog
}  // namespace ase::explorer

#pragma once

/**
 * @file        icons.hpp
 * @brief       NerdFont glyph lookup and Pango markup for file/folder rows
 * @description Resolves filenames to a coloured NerdFont glyph via the generated
 *              sha-web-console/file-icons.hpp tables. Produces Pango markup that
 *              the tree view's icon Label widget can render. The icon font names
 *              and point size are the single source of truth for the whole app.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <string>

namespace ase::explorer::icons {

/** NerdFont family fallback chain used everywhere icons are rendered. */
constexpr const char* ICON_FONT = "FiraCode Nerd Font, JetBrainsMono Nerd Font Mono";

/** Icon glyph point size (Pango uses 1024ths of a point internally). */
constexpr int ICON_FONT_SIZE = 14;

/** Text font used for submodule badges. */
constexpr const char* TEXT_FONT = "Fira Code";

/** Resolved icon: codepoint plus 0.0-1.0 RGB components. */
struct ResolvedIcon {
    char32_t glyph;
    double r;
    double g;
    double b;
};

/** Look up the icon for a filename (exact match, extension match, fallback). */
ResolvedIcon get_file_icon(const std::string& filename);

/** Look up the icon for a directory (expanded/collapsed, submodule variant). */
ResolvedIcon get_folder_icon(bool expanded, bool is_submodule);

/** Render a ResolvedIcon as Pango markup that renders the glyph in its colour. */
std::string icon_markup(const ResolvedIcon& icon);

}  // namespace ase::explorer::icons

#pragma once

/**
 * @file        icons.hpp
 * @brief       NerdFont glyph lookup and Pango markup for file/folder rows
 * @description Resolves filenames to a coloured NerdFont glyph via the generated
 *              sha-web-console/file-icons.hpp tables. Also exposes helpers that
 *              wrap the SAME NerdFont rendering for arbitrary semantic UI icons
 *              (settings cog, trash, search, …) sourced from ui_icons.hpp. No
 *              `*-symbolic` gnome-icon-theme names are used anywhere in the
 *              client — every glyph traces back to icon-definitions.ts (SSOT).
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <string>

#include <stdint.h>

typedef struct _GtkWidget GtkWidget;

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

/**
 * Render an arbitrary NerdFont glyph as Pango markup with the given 0xRRGGBB
 * colour and a custom point size. Used for UI icons sourced from ui_icons.hpp.
 */
std::string glyph_markup(char32_t glyph, uint32_t rgb_color, int point_size);

/**
 * Build a GtkLabel widget that displays a NerdFont glyph rendered in the
 * NerdFont family with the given colour and size. The returned widget can be
 * appended to any container or stuffed inside a button.
 */
GtkWidget* make_glyph_label(char32_t glyph, uint32_t rgb_color, int point_size);

/**
 * Build a flat GtkButton whose child is a glyph label. Convenience for
 * header-bar / chip-strip / row-suffix buttons. The tooltip is optional.
 */
GtkWidget* make_glyph_button(char32_t glyph,
                             uint32_t rgb_color,
                             int point_size,
                             const std::string& tooltip = "");

}  // namespace ase::explorer::icons

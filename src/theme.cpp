/**
 * @file        theme.cpp
 * @brief       Implementation for theme.hpp
 * @description Pure string concatenation against the ase::colors constants.
 *              Every selector maps a single colour so tweaking the palette
 *              propagates automatically.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <explorer/theme.hpp>

#include <ase/utils/strops.hpp>

#include "colors.hpp"

#include <cstdint>

namespace ase::explorer::theme {

namespace {

std::string css_hex(uint32_t c) {
    char buf[8];
    ase::utils::format_hex_color(buf, sizeof(buf), c);
    return buf;
}

}  // namespace

std::string generate_css() {
    using namespace ase::colors;
    return
        "* { border-radius: 0; }\n"
        "window { background-color: " + css_hex(SURFACE_0_PANEL) + "; color: " + css_hex(TEXT_PRIMARY) + "; }\n"
        "headerbar { background: linear-gradient(to right, " + css_hex(SURFACE_1) + ", " + css_hex(SURFACE_2) + ");"
            " border-bottom: 1px solid " + css_hex(BORDER_SECONDARY) + ";"
            " color: " + css_hex(PANEL_CYAN) + "; min-height: 32px; }\n"
        "listview { background-color: transparent; color: " + css_hex(TEXT_PRIMARY) + ";"
            " font-family: 'Fira Code', monospace; font-size: 12px; }\n"
        // Row: no VERTICAL padding so adjacent rows touch at their top/bottom
        // edges. This lets the tree-guide DrawingArea (first child) paint
        // continuous vertical guide rails across row boundaries. Vertical
        // spacing is re-introduced as margin on the inner content widgets so
        // the visual row separation is preserved. Horizontal padding stays
        // so the leading/trailing content has breathing room.
        "listview > row { padding: 0 8px; min-height: 28px; }\n"
        "listview > row:hover { background-color: " + css_hex(SURFACE_2_HOVER) + "; }\n"
        "listview > row:selected { background-color: " + css_hex(SURFACE_2) + "; color: " + css_hex(PANEL_CYAN) + "; }\n"
        "treeexpander { min-width: 20px; }\n"
        "popover, popover > contents, menu, menuitem { background-color: " + css_hex(SURFACE_1) + ";"
            " color: " + css_hex(TEXT_PRIMARY) + "; }\n"
        "button { background-color: " + css_hex(SURFACE_3) + "; color: " + css_hex(TEXT_PRIMARY) + "; }\n"
        "button:hover { background-color: " + css_hex(SURFACE_4) + "; }\n"
        "entry, searchbar, search > entry { background-color: " + css_hex(SURFACE_1) + ";"
            " color: " + css_hex(TEXT_PRIMARY) + "; border: 1px solid " + css_hex(BORDER_PRIMARY) + "; }\n"
        "scrollbar slider { background-color: " + css_hex(SURFACE_4) + "; }\n"
        "scrollbar slider:hover { background-color: " + css_hex(SURFACE_5) + "; }\n";
}

}  // namespace ase::explorer::theme

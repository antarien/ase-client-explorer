/**
 * @file        theme.cpp
 * @brief       Implementation for theme.hpp
 * @description Pure string concatenation against the ase::colors AND
 *              ase::theme constants. Mirrors the dashboard SSOT — see
 *              sha-web-styles/src/dash/tokens.css for the alias chain:
 *
 *                --dash-surface-0       = SURFACE_0          (pure black)
 *                --dash-surface-2       = SURFACE_3          (#1A1A1A header bg)
 *                --dash-text-primary    = TEXT_DEFAULT       (#9A9A9A)
 *                --dash-text-muted      = TEXT_LABEL         (#5A5A5A)
 *                --dash-border-primary  = BORDER_PRIMARY     (#2A2A2A)
 *                --dash-border-secondary= BORDER_SECONDARY   (#1F1F1F)
 *
 *              Tab style mirrors `.dashboard-tab` + active state from
 *              ase-web-wizard/src/steps/PluginSelection.tsx (active border
 *              = the panel's accent colour, here PANEL_CYAN to match the
 *              tree-row selected state already used by the explorer).
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <explorer/theme.hpp>

#include <ase/utils/strops.hpp>

#include "colors.hpp"
#include "design_tokens.hpp"

#include <cstdint>
#include <string>

namespace ase::explorer::theme {

namespace {

std::string css_hex(uint32_t c) {
    char buf[8];
    ase::utils::format_hex_color(buf, sizeof(buf), c);
    return buf;
}

std::string px(int v) { return std::to_string(v) + "px"; }

}  // namespace

std::string generate_css() {
    using namespace ase::colors;
    namespace t = ase::theme;

    // ── Color tokens — mirror dash/tokens.css aliases ───────────────
    const std::string surface_0     = css_hex(SURFACE_0);          // pure black, dashboard bg
    const std::string surface_1     = css_hex(SURFACE_1);          // elevated card
    const std::string surface_2     = css_hex(SURFACE_2);          // popover, selection
    const std::string surface_2_h   = css_hex(SURFACE_2_HOVER);
    const std::string surface_3     = css_hex(SURFACE_3);          // dashboard "header" bg
    const std::string surface_4     = css_hex(SURFACE_4);
    const std::string surface_5     = css_hex(SURFACE_5);
    const std::string text_primary  = css_hex(TEXT_DEFAULT);       // dash-text-primary
    const std::string text_muted    = css_hex(TEXT_LABEL);         // dash-text-muted
    const std::string text_hover    = css_hex(TEXT_HOVER);
    const std::string text_light    = css_hex(TEXT_LIGHT);
    const std::string border_primary= css_hex(BORDER_PRIMARY);     // dash-border-primary
    const std::string border_sec    = css_hex(BORDER_SECONDARY);   // dash-border-secondary
    const std::string accent        = css_hex(PANEL_CYAN);         // explorer accent (matches tree row selected)

    // ── Design tokens (cached as CSS strings for readability) ────────
    const std::string font_mono     = t::font_mono;
    const std::string fs_xs         = px(t::font_xs);     // 10
    const std::string fs_sm         = px(t::font_sm);     // 11
    const std::string fs_md         = px(t::font_md);     // 12
    const std::string fs_lg         = px(t::font_lg);     // 14
    const std::string sp_xs         = px(t::space_xs);    // 4
    const std::string sp_sm         = px(t::space_sm);    // 8
    const std::string sp_md         = px(t::space_md);    // 16
    const std::string sp_lg         = px(t::space_lg);    // 24
    const std::string sp_xl         = px(t::space_xl);    // 32
    // Composite values frequently needed but absent from the SSOT scale.
    // Compose them from the official tokens so they still trace back.
    const std::string sp_2          = px(t::space_xs / 2);             // 2
    const std::string sp_6          = px(t::space_sm - t::space_xs/2); // 6
    const std::string sp_12         = px(t::space_sm + t::space_xs);   // 12
    const std::string sp_20         = px(t::space_md + t::space_xs);   // 20

    return
        "* { border-radius: 0; }\n"
        "window { background-color: " + surface_0 + "; color: " + text_primary + ";"
            " font-family: " + font_mono + "; }\n"
        "headerbar { background-color: " + surface_3 + ";"
            " border-bottom: 1px solid " + border_sec + "; color: " + text_primary + ";"
            " min-height: " + sp_xl + "; }\n"

        // ── Tree list (file browser) ─────────────────────────────────
        "listview { background-color: transparent; color: " + text_primary + ";"
            " font-family: " + font_mono + "; font-size: " + fs_md + "; }\n"
        "listview > row { padding: 0 " + sp_sm + "; min-height: " + sp_lg + "; }\n"
        "listview > row:hover { background-color: " + surface_2_h + "; }\n"
        "listview > row:selected { background-color: " + surface_2 + "; color: " + accent + "; }\n"
        "treeexpander { min-width: " + sp_md + "; }\n"

        "popover, popover > contents, menu, menuitem { background-color: " + surface_1 + ";"
            " color: " + text_primary + "; }\n"
        "button { background-color: " + surface_3 + "; color: " + text_primary + "; }\n"
        "button:hover { background-color: " + surface_4 + "; }\n"
        "entry, searchbar, search > entry { background-color: " + surface_1 + ";"
            " color: " + text_primary + "; border: 1px solid " + border_primary + "; }\n"
        "scrollbar slider { background-color: " + surface_4 + "; }\n"
        "scrollbar slider:hover { background-color: " + surface_5 + "; }\n"

        // ── Cockpit tab-bar wrapper (left-aligned, in body) ──────────
        // Mirrors .dashboard-tab-bar from sha-web-styles/src/dash/atoms.css
        ".ase-cfg-tab-bar { background-color: " + surface_0 + ";"
            " padding: 0 " + sp_md + ";"
            " border-bottom: 1px solid " + border_sec + "; }\n"

        // ── ViewSwitcher tab styling — mirrors .dashboard-tab ────────
        // dashboard-tab: padding 8 16, font-size --ase-font-sm (11),
        // text-transform uppercase, color text-muted/text-primary,
        // active border-bottom 1px panel accent. Letter-spacing is
        // unsupported by GTK CSS so we omit it (Pango markup elsewhere).
        "viewswitcher { background: transparent; }\n"
        "viewswitcher > button { background: transparent; border: none;"
            " border-radius: 0; border-bottom: 1px solid transparent;"
            " padding: " + sp_sm + " " + sp_md + "; min-height: 0; box-shadow: none;"
            " color: " + text_muted + "; font-size: " + fs_sm + ";"
            " text-transform: uppercase; }\n"
        "viewswitcher > button label { font-size: " + fs_sm + ";"
            " text-transform: uppercase; }\n"
        "viewswitcher > button:hover { background: transparent; color: " + text_primary + "; }\n"
        "viewswitcher > button:checked { background: transparent;"
            " border-bottom-color: " + accent + "; color: " + text_light + "; }\n"

        // ── Adwaita preference rows — scoped to preferencespage ──────
        "preferencespage { background-color: " + surface_0 + "; }\n"
        "preferencespage row.activatable,"
        " preferencespage row.entry,"
        " preferencespage row.combo,"
        " preferencespage row.switch {"
            " background-color: " + surface_1 + "; color: " + text_primary + ";"
            " border: 1px solid " + border_sec + "; }\n"
        "preferencespage row.activatable:hover { background-color: " + surface_2_h + "; }\n"

        // ── Modal Open-With dialog body ──────────────────────────────
        ".ase-openwith { background-color: " + surface_0 + "; }\n"
        ".ase-openwith-header { color: " + text_muted + "; font-size: " + fs_sm + ";"
            " padding: " + sp_md + " " + sp_md + ";"
            " border-bottom: 1px solid " + border_sec + ";"
            " text-transform: uppercase; }\n"
        ".ase-openwith listview { background: transparent; }\n"
        ".ase-openwith listview > row { padding: " + sp_6 + " " + sp_12 + ";"
            " min-height: " + sp_xl + "; }\n"
        ".ase-openwith listview > row:selected { background-color: " + surface_2 + ";"
            " color: " + text_light + "; }\n"
        ".ase-openwith-app-name { color: " + text_light + "; font-size: " + fs_md + "; }\n"
        ".ase-openwith-app-exec { color: " + text_muted + "; font-size: " + fs_xs + "; }\n"

        // ── Dashboard-style settings dialog (dense, no inner padding) ──
        ".ase-cfg-window { background-color: " + surface_0 + "; }\n"

        // Section header strip — full width, hairline below in border-secondary
        // (matches dashboard-tab-bar bottom border) and a tinted background
        // (matches dashboard "header" surface = SURFACE_3).
        ".ase-cfg-section-title { color: " + text_muted + "; background-color: " + surface_3 + ";"
            " font-size: " + fs_sm + "; font-weight: 600;"
            " padding: " + sp_sm + " " + sp_md + ";"
            " border-bottom: 1px solid " + border_sec + ";"
            " text-transform: uppercase; }\n"

        // Individual setting cell — flush, hairline divider only
        ".ase-cfg-cell { padding: " + sp_sm + " " + sp_md + ";"
            " border-right: 1px solid " + border_sec + ";"
            " border-bottom: 1px solid " + border_sec + ";"
            " background-color: " + surface_0 + "; }\n"
        ".ase-cfg-cell:hover { background-color: " + surface_2_h + "; }\n"
        ".ase-cfg-cell-label { color: " + text_muted + "; font-size: " + fs_xs + ";"
            " text-transform: uppercase; }\n"
        ".ase-cfg-cell-value { color: " + text_light + "; font-size: " + fs_md + "; }\n"

        // Cockpit pane separator — hairline border-secondary, not accent
        ".ase-cfg-pane { background-color: " + surface_0 + "; }\n"
        ".ase-cfg-pane-divider { background-color: " + border_sec + "; min-width: 1px; }\n"

        // Associations table rows
        ".ase-cfg-map-row { padding: " + sp_sm + " " + sp_md + ";"
            " border-bottom: 1px solid " + border_sec + ";"
            " background-color: " + surface_0 + "; }\n"
        ".ase-cfg-map-row:hover { background-color: " + surface_2_h + "; }\n"
        ".ase-cfg-map-row:selected { background-color: " + surface_2 + "; }\n"
        ".ase-cfg-map-ext { color: " + accent + "; font-size: " + fs_md + "; font-weight: bold; }\n"
        ".ase-cfg-map-app { color: " + text_light + "; font-size: " + fs_md + "; }\n"
        ".ase-cfg-map-exec { color: " + text_muted + "; font-size: " + fs_xs + "; }\n"

        // Application picker rows on the right pane
        ".ase-cfg-app-row { padding: " + sp_6 + " " + sp_12 + ";"
            " border-bottom: 1px solid " + border_sec + "; }\n"
        ".ase-cfg-app-row:hover { background-color: " + surface_2_h + "; }\n"
        ".ase-cfg-app-row:selected { background-color: " + surface_2 + "; }\n"
        ".ase-cfg-app-name { color: " + text_light + "; font-size: " + fs_md + "; }\n"
        ".ase-cfg-app-id { color: " + text_muted + "; font-size: " + fs_xs + "; }\n"

        // Extension rows (left pane of Associations) — file census output
        ".ase-cfg-ext-row { padding: " + sp_6 + " " + sp_12 + ";"
            " border-bottom: 1px solid " + border_sec + "; }\n"
        ".ase-cfg-ext-row:hover { background-color: " + surface_2_h + "; }\n"
        ".ase-cfg-ext-row:selected { background-color: " + surface_2 + "; }\n"
        ".ase-cfg-ext-name { color: " + accent + "; font-size: " + fs_md + "; font-weight: bold; }\n"
        ".ase-cfg-ext-count { color: " + text_muted + "; font-size: " + fs_xs + "; }\n"
        ".ase-cfg-ext-mapped { color: " + text_light + "; font-size: " + fs_xs + "; }\n"

        // Active mapping chips at the bottom — bordered in border-primary
        ".ase-cfg-chip { background-color: " + surface_2 + ";"
            " border: 1px solid " + border_primary + ";"
            " padding: " + sp_xs + " " + sp_sm + ";"
            " margin: " + sp_xs + "; }\n"
        ".ase-cfg-chip-ext { color: " + accent + "; font-size: " + fs_xs + "; font-weight: bold; }\n"
        ".ase-cfg-chip-arrow { color: " + text_muted + "; font-size: " + fs_xs + "; }\n"
        ".ase-cfg-chip-app { color: " + text_light + "; font-size: " + fs_xs + "; }\n"

        // Hint shown in left-pane footer / right-pane footer
        ".ase-cfg-hint { color: " + text_muted + "; font-size: " + fs_xs + ";"
            " padding: " + sp_sm + " " + sp_md + ";"
            " border-top: 1px solid " + border_sec + "; }\n";
}

}  // namespace ase::explorer::theme

/**
 * @file        icons.cpp
 * @brief       Implementation for icons.hpp
 * @description Three-tier lookup: exact filename match, lowercased extension
 *              match, then UNKNOWN fallback. UTF-8 encoding of the glyph is
 *              inlined to avoid pulling in iconv just for one codepoint.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <explorer/icons.hpp>

#include <ase/utils/strops.hpp>

#include "file-icons.hpp"

#include <gtk/gtk.h>

#include <cctype>
#include <cstdint>

namespace ase::explorer::icons {

namespace {

ResolvedIcon unpack_color(const ase::file_icons::FileIcon& icon) {
    uint32_t c = icon.color;
    return {
        icon.glyph,
        ((c >> 16) & 0xFF) / 255.0,
        ((c >>  8) & 0xFF) / 255.0,
        ((c      ) & 0xFF) / 255.0,
    };
}

std::string to_utf8(char32_t cp) {
    std::string s;
    if (cp < 0x80) {
        s += static_cast<char>(cp);
    } else if (cp < 0x800) {
        s += static_cast<char>(0xC0 | (cp >> 6));
        s += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        s += static_cast<char>(0xE0 | (cp >> 12));
        s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        s += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        s += static_cast<char>(0xF0 | (cp >> 18));
        s += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        s += static_cast<char>(0x80 | (cp & 0x3F));
    }
    return s;
}

}  // namespace

ResolvedIcon get_file_icon(const std::string& filename) {
    // 1. Exact filename match (highest priority).
    for (int i = 0; i < ase::file_icons::NAME_ICONS_COUNT; ++i) {
        if (filename == ase::file_icons::NAME_ICONS[i].pattern) {
            return unpack_color(ase::file_icons::NAME_ICONS[i]);
        }
    }

    // 2. Extension match, case-insensitive.
    auto dot = filename.rfind('.');
    if (dot != std::string::npos) {
        auto ext = filename.substr(dot);
        for (auto& c : ext) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        for (int i = 0; i < ase::file_icons::EXT_ICONS_COUNT; ++i) {
            if (ext == ase::file_icons::EXT_ICONS[i].pattern) {
                return unpack_color(ase::file_icons::EXT_ICONS[i]);
            }
        }
    }

    // 3. Fallback.
    return unpack_color(ase::file_icons::UNKNOWN);
}

ResolvedIcon get_folder_icon(bool expanded, bool is_submodule) {
    if (is_submodule) return unpack_color(ase::file_icons::SUBMODULE);
    return expanded ? unpack_color(ase::file_icons::FOLDER_OPEN)
                    : unpack_color(ase::file_icons::FOLDER_CLOSED);
}

std::string icon_markup(const ResolvedIcon& icon) {
    char color_hex[8];
    ase::utils::format_hex_rgb(color_hex, sizeof(color_hex),
        static_cast<uint32_t>(icon.r * 255),
        static_cast<uint32_t>(icon.g * 255),
        static_cast<uint32_t>(icon.b * 255));
    return "<span font_family='" + std::string(ICON_FONT) + "' font_size='"
         + std::to_string(ICON_FONT_SIZE * 1024) + "' foreground='" + std::string(color_hex) + "'>"
         + to_utf8(icon.glyph) + "</span>";
}

std::string glyph_markup(char32_t glyph, uint32_t rgb_color, int point_size) {
    char color_hex[8];
    ase::utils::format_hex_rgb(color_hex, sizeof(color_hex),
        (rgb_color >> 16) & 0xFF,
        (rgb_color >>  8) & 0xFF,
         rgb_color        & 0xFF);
    return "<span font_family='" + std::string(ICON_FONT) + "' font_size='"
         + std::to_string(point_size * 1024) + "' foreground='" + std::string(color_hex) + "'>"
         + to_utf8(glyph) + "</span>";
}

GtkWidget* make_glyph_label(char32_t glyph, uint32_t rgb_color, int point_size) {
    GtkWidget* label = gtk_label_new(nullptr);
    const std::string markup = glyph_markup(glyph, rgb_color, point_size);
    gtk_label_set_markup(GTK_LABEL(label), markup.c_str());
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    return label;
}

GtkWidget* make_glyph_button(char32_t glyph,
                             uint32_t rgb_color,
                             int point_size,
                             const std::string& tooltip)
{
    GtkWidget* btn = gtk_button_new();
    GtkWidget* label = make_glyph_label(glyph, rgb_color, point_size);
    gtk_button_set_child(GTK_BUTTON(btn), label);
    gtk_widget_add_css_class(btn, "flat");
    if (!tooltip.empty()) gtk_widget_set_tooltip_text(btn, tooltip.c_str());
    return btn;
}

}  // namespace ase::explorer::icons

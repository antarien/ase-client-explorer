#pragma once

/**
 * @file        explorer_settings.hpp
 * @brief       Per-user explorer preferences persisted as JSON
 * @description Companion to FileAssociations: a generic JSON store under
 *              ~/.config/ase/explorer/settings.json holding all explorer
 *              prefs that are NOT extension associations (max breadcrumb
 *              segments, future toggles for hidden files / single-click /
 *              terminal emulator, etc.).
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <filesystem>

namespace ase::explorer {

class ExplorerSettings {
public:
    static ExplorerSettings load();
    void save() const;

    int  breadcrumb_max_segments() const noexcept { return m_breadcrumb_max; }
    void set_breadcrumb_max_segments(int n) noexcept;

    const std::filesystem::path& path() const noexcept { return m_path; }

private:
    static std::filesystem::path default_store_path();

    int                   m_breadcrumb_max = 5;
    std::filesystem::path m_path;
};

}  // namespace ase::explorer

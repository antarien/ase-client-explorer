#pragma once

/**
 * @file        file_associations.hpp
 * @brief       Persistent extension -> desktop-id mapping store
 * @description User-defined file-extension to desktop-application mapping,
 *              persisted as JSON under ~/.config/ase/explorer/. Lookups are
 *              strictly explicit: if no mapping exists for an extension, the
 *              return is empty — there is NO fallback to Gio defaults, no
 *              guess, no substitute. Callers that want the OS default must
 *              ask Gio themselves; this store does not blur the line.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace ase::explorer {

class FileAssociations {
public:
    /** Load from the on-disk store (creates an empty store if the file does not exist). */
    static FileAssociations load();

    /** Persist to the on-disk store, creating parent directories as needed. */
    void save() const;

    /**
     * Look up an extension. Accepts both ".cpp" and "cpp"; case-insensitive.
     * Returns the stored desktop id (e.g. "sublime_text.desktop") or an
     * empty string if no mapping exists. NEVER falls back.
     */
    std::string lookup(const std::string& extension) const;

    /** Insert or replace a mapping. Caller must save() to persist. */
    void set(const std::string& extension, const std::string& desktop_id);

    /** Remove a mapping. No-op if the extension is not present. */
    void remove(const std::string& extension);

    /** Snapshot of all current mappings sorted by extension, for UI rendering. */
    std::vector<std::pair<std::string, std::string>> all() const;

    /** Path used by save()/load() — exposed for diagnostics. */
    const std::filesystem::path& path() const noexcept { return m_path; }

private:
    static std::string normalize_extension(const std::string& ext);
    static std::filesystem::path default_store_path();

    std::map<std::string, std::string> m_map;
    std::filesystem::path m_path;
};

}  // namespace ase::explorer

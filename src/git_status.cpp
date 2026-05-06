/**
 * @file        git_status.cpp
 * @brief       Implementation of StatusCache
 * @description Backs the public lookups with three libcuckoo maps:
 *
 *                file_map    : abs_path → FileStatus
 *                dir_map     : abs_path → DirRollup
 *                summary_map : repo_root → SubmoduleSummary
 *                owned_map   : repo_root → vector<abs_path>
 *
 *              file_map / dir_map are read by the GTK bind callback on
 *              the main thread without any lock. Workers publish via
 *              publish_repo() which is multi-writer safe (libcuckoo
 *              handles per-bucket synchronisation).
 *
 *              owned_map tracks which absolute paths the previous scan
 *              of a repo inserted, so the next publish can erase exactly
 *              those keys (libcuckoo has no prefix-erase). Without this
 *              tracking the cache would leak rows for deleted files.
 *
 *              The on_updated signal is bridged to the GTK main thread
 *              through Glib::signal_idle so the bind callback never
 *              races with a concurrent publish on the file_map.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <explorer/git_status.hpp>

#include <ase/adp/libcuckoo/map.hpp>

#include <glibmm/main.h>

#include <utility>
#include <vector>

namespace ase::explorer::git {

struct StatusCache::Impl {
    using FileMap    = ase::adp::libcuckoo::Map<std::string, FileStatus>;
    using DirMap     = ase::adp::libcuckoo::Map<std::string, DirRollup>;
    using SummaryMap = ase::adp::libcuckoo::Map<std::string, SubmoduleSummary>;
    using OwnedMap   = ase::adp::libcuckoo::Map<std::string, std::vector<std::string>>;

    FileMap    file_map;
    DirMap     dir_map;
    SummaryMap summary_map;
    OwnedMap   owned_map;
};

StatusCache::StatusCache()
    : m_impl(std::make_unique<Impl>())
{}

StatusCache::~StatusCache() = default;

void StatusCache::set_root(const std::string& /*root_path*/) {
    // The cache itself is repo-agnostic — set_root() exists in the
    // public API so callers have one entry point that resets state
    // and (eventually) re-emits on_updated. Today the call only
    // clears the maps; the actual repo enumeration and scan
    // dispatching is done by ScanPool::schedule_full_rescan() which
    // the window invokes alongside this.
    clear();
    // Fire on_updated so the tree view repaints with empty badges
    // until the first scan completes.
    m_on_updated.emit();
}

void StatusCache::clear() {
    m_impl->file_map.clear();
    m_impl->dir_map.clear();
    m_impl->summary_map.clear();
    m_impl->owned_map.clear();
}

FileState StatusCache::file_state(const std::string& abs_path) const {
    FileStatus s;
    if (m_impl->file_map.find(abs_path, s)) {
        return s.state;
    }
    return FileState::Clean;
}

DirRollup StatusCache::dir_rollup(const std::string& abs_path) const {
    DirRollup r;
    if (m_impl->dir_map.find(abs_path, r)) {
        return r;
    }
    return {};
}

bool StatusCache::dir_has_dirty(const std::string& abs_path) const {
    DirRollup r;
    if (m_impl->dir_map.find(abs_path, r)) {
        return r.any();
    }
    return false;
}

SubmoduleSummary StatusCache::submodule_summary(const std::string& abs_path) const {
    SubmoduleSummary s;
    if (m_impl->summary_map.find(abs_path, s)) {
        return s;
    }
    return {};
}

std::vector<std::string> StatusCache::known_submodules() const {
    // libcuckoo's locked_table() gives a const-iterable view at the
    // cost of stalling concurrent writers for the duration. Used here
    // only when the user toggles VCS-filter mode, so the freeze is
    // measured in microseconds.
    std::vector<std::string> out;
    auto lt = m_impl->summary_map.lock_table();
    out.reserve(lt.size());
    for (const auto& kv : lt) {
        out.push_back(kv.first);
    }
    return out;
}

void StatusCache::publish_repo(
    const std::string& repo_root,
    const SubmoduleSummary& summary,
    std::vector<std::pair<std::string, FileStatus>> file_entries,
    std::vector<std::pair<std::string, DirRollup>>  dir_entries)
{
    // 1. Erase the keys this repo's previous scan owned. We use
    //    update_fn to atomically read-and-clear the owned-paths
    //    vector, so a parallel publish for the SAME repo (which
    //    should not happen but is defensive) would not double-erase.
    std::vector<std::string> previously_owned;
    m_impl->owned_map.update_fn(repo_root,
        [&previously_owned](std::vector<std::string>& v) {
            previously_owned = std::move(v);
            v.clear();
        });
    for (const auto& p : previously_owned) {
        m_impl->file_map.erase(p);
        m_impl->dir_map.erase(p);
    }

    // 2. Insert / overwrite the new entries. libcuckoo's
    //    insert_or_assign returns true on insert, false on overwrite —
    //    we don't care which.
    std::vector<std::string> new_owned;
    new_owned.reserve(file_entries.size() + dir_entries.size());
    for (auto& kv : file_entries) {
        m_impl->file_map.insert_or_assign(kv.first, kv.second);
        new_owned.push_back(kv.first);
    }
    for (auto& kv : dir_entries) {
        m_impl->dir_map.insert_or_assign(kv.first, kv.second);
        new_owned.push_back(kv.first);
    }
    m_impl->owned_map.insert_or_assign(repo_root, std::move(new_owned));

    // 3. Replace the per-repo summary.
    m_impl->summary_map.insert_or_assign(repo_root, summary);

    // 4. Fire on_updated on the GTK main thread. Glib::signal_idle
    //    is the canonical bridge from worker threads back to the UI
    //    main loop; the lambda captures a pointer to the signal which
    //    lives as long as the StatusCache, so the callback is safe
    //    against teardown ordering as long as the cache outlives any
    //    in-flight publish (the explorer holds it as a member of
    //    ExplorerWindow, so this is the case).
    auto* sig = &m_on_updated;
    Glib::signal_idle().connect_once([sig]() { sig->emit(); });
}

}  // namespace ase::explorer::git

#pragma once

/**
 * @file        git_status.hpp
 * @brief       Concurrent VCS-status cache for the explorer tree
 * @description Holds the per-file git status of every (sub-)repository
 *              that lives under the current explorer root. Built so the
 *              tree-view bind callback (the hot path) does an O(1) read
 *              with NO locks, while N background scanner threads each
 *              publish their per-file findings as O(1) point-updates.
 *
 *              Two libcuckoo-backed maps:
 *                  StatusMap     : abs path  → FileStatus (per file)
 *                  RollupMap     : abs path  → DirRollup  (per dir)
 *
 *              The dir rollup is precomputed at scan time so the VCS
 *              filter (only-show-dirty mode) can prune subtrees in O(1)
 *              without recursing the file map. Per-submodule
 *              SubmoduleSummary is exposed as a separate read for the
 *              right-aligned submodule badge.
 *
 *              Lifecycle:
 *                StatusCache::set_root(...) seeds the list of
 *                repositories (root + submodules). The scan pool then
 *                dispatches workers; each completion calls publish_*()
 *                here. Consumers connect to on_updated() (Glib idle-
 *                friendly signal) to repaint visible rows. Filter mode
 *                is a single repopulate; per-row badges piggyback on
 *                gtk's natural rebind cycle.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <ase/adp/libcuckoo/map.hpp>

#include <sigc++/signal.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ase::explorer::git {

/**
 * Per-file status. Mirrors the libgit2 adapter's StatusFlags subset
 * the explorer actually needs to render, collapsed into a single
 * primary state. Files that carry multiple bits (e.g. modified-and-
 * staged) are normalised here once in the worker so the bind callback
 * does not have to.
 */
enum class FileState : uint8_t {
    Clean        = 0,
    Modified     = 1,   ///< worktree or index modified / typechanged
    Added        = 2,   ///< new file in the index
    Deleted      = 3,   ///< deleted in worktree or index
    Renamed      = 4,
    Untracked    = 5,   ///< worktree-new, not yet added
    Conflicted   = 6,
    Ignored      = 7,
};

struct FileStatus {
    FileState state = FileState::Clean;
};

/**
 * Per-directory aggregate count. Computed at scan time so the VCS
 * filter (dir_has_dirty) is O(1). Counts are direct + recursive: the
 * rollup at /repo/src/foo/ counts every dirty file under that prefix.
 */
struct DirRollup {
    uint32_t modified   = 0;
    uint32_t added      = 0;
    uint32_t deleted    = 0;
    uint32_t renamed    = 0;
    uint32_t untracked  = 0;
    uint32_t conflicted = 0;

    uint32_t total() const noexcept {
        return modified + added + deleted + renamed + untracked + conflicted;
    }
    bool any() const noexcept { return total() > 0; }
};

/**
 * Per-(sub)repository summary as displayed on the submodule badge.
 * `head_short` is the 7-char SHA at the time of the last scan — the
 * scan pool uses it as a cheap change-marker (a flip implies the work
 * tree is on a new commit, so badges are stale).
 */
struct SubmoduleSummary {
    std::string head_short;
    DirRollup   rollup;
    bool        scanned = false;   ///< false until first scan completed
};

class StatusCache {
public:
    StatusCache();
    ~StatusCache();

    StatusCache(const StatusCache&)            = delete;
    StatusCache& operator=(const StatusCache&) = delete;

    /**
     * Reset the set of (sub-)repositories the cache tracks. Caller
     * passes the project root, which is enumerated for submodules
     * via libgit2; the scan pool then schedules an initial sweep.
     * Replaces any prior root.
     */
    void set_root(const std::string& root_path);

    /** Drop everything. on_updated does NOT fire (set_root() does). */
    void clear();

    // ── Hot-path reads (lock-free O(1)) ─────────────────────────────

    /** Lookup a file's state. Defaults to Clean when not found. */
    FileState file_state(const std::string& abs_path) const;

    /** Lookup a folder's aggregate. Empty rollup when not found. */
    DirRollup dir_rollup(const std::string& abs_path) const;

    /**
     * True if the folder OR any descendant is dirty. Reads only the
     * dir_rollup map — no recursion needed because rollups are
     * precomputed transitively at scan time.
     */
    bool dir_has_dirty(const std::string& abs_path) const;

    /**
     * Lookup the per-submodule summary (badge: M3 ?5 …). The repo
     * root is also indexed under its own absolute path, so the same
     * call works whether the user is hovering a registered submodule
     * row or the project root row.
     */
    SubmoduleSummary submodule_summary(const std::string& abs_path) const;

    /** Every absolute submodule path the cache currently knows. */
    std::vector<std::string> known_submodules() const;

    // ── Scanner-side writes (called from worker threads) ────────────

    /**
     * Replace the per-(sub)repo state in one atomic batch. Called by
     * the scan pool when a worker thread finishes a single repo's
     * git_status_list walk. The cache erases the previous entries
     * for `repo_root` first, then point-inserts each new entry into
     * the libcuckoo maps (multi-writer safe across repos).
     *
     * Triggers on_updated() once the publish has completed.
     */
    void publish_repo(
        const std::string& repo_root,
        const SubmoduleSummary& summary,
        std::vector<std::pair<std::string, FileStatus>> file_entries,
        std::vector<std::pair<std::string, DirRollup>>  dir_entries);

    // ── UI-side notification ───────────────────────────────────────

    /**
     * Fired on the GTK main thread (via Glib::signal_idle) every time
     * a worker publishes a fresh repo snapshot. The tree view connects
     * here to call gtk_widget_queue_draw on its list view — visible
     * rows naturally rebind and pick up the new badges in O(visible).
     */
    sigc::signal<void()>& on_updated() noexcept { return m_on_updated; }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    sigc::signal<void()> m_on_updated;
};

// ── Pango-markup helpers shared by every consumer that displays a
//    file or directory together with its VCS state (tree rows,
//    breadcrumb segments, future tooltips). The exact glyphs +
//    panel colours are defined in git_status.cpp so SSOT updates
//    propagate to every consumer at once.

/** 1-letter Pango span for the file's primary state, "" when Clean/Ignored. */
std::string vcs_file_badge_markup(FileState s);

/** "M3 A1 ?5" Pango spans (per-category panel colour), "" when empty. */
std::string vcs_dir_badge_markup(const DirRollup& r);

}  // namespace ase::explorer::git

#pragma once

/**
 * @file        git_scan_pool.hpp
 * @brief       Bounded thread pool that scans (sub-)repositories via libgit2
 * @description Owns a small std::thread workforce (default = hardware_
 *              concurrency, capped at 16) and a FIFO of repo paths to
 *              scan. Each worker:
 *                1. opens the libgit2 Repository for its job's path,
 *                2. runs scan() with the configured StatusOptions,
 *                3. translates StatusEntries → FileStatus + DirRollup,
 *                4. calls StatusCache::publish_repo() with the result.
 *
 *              Per-repo handles are NOT cached across jobs in this
 *              first pass — open+scan+free per job is ~2-10 ms and
 *              keeps the worker logic stateless. A future iteration
 *              can hold persistent Repository instances per worker
 *              keyed by path; the public API here does not change.
 *
 *              Cancellation: schedule_full_rescan() drains the pending
 *              queue first so an in-flight scan completes but no
 *              stale jobs run after a clear(). stop() joins workers.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <explorer/git_status.hpp>

#include <atomic>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace ase::explorer::git {

class ScanPool {
public:
    explicit ScanPool(StatusCache& cache);
    ~ScanPool();

    ScanPool(const ScanPool&)            = delete;
    ScanPool& operator=(const ScanPool&) = delete;

    /**
     * Spawn worker threads. Idempotent — safe to call once at startup
     * after the libgit2 Library guard exists. Workers idle on a
     * condition variable; they wake on every schedule_*() call.
     */
    void start(std::size_t thread_count = 0);

    /**
     * Drain pending work + join workers. Called from the explorer
     * window destructor. Safe to call multiple times.
     */
    void stop();

    /**
     * Enqueue a single repository for re-scan. Called by the file
     * watcher's owning-submodule resolver when a relevant path
     * changes (.git/index mtime, worktree file modify, …).
     * Coalesces: if a job for this path is already in the queue
     * the second request is dropped.
     */
    void schedule_repo(const std::string& abs_path);

    /**
     * Enumerate the project root + every (nested) submodule once and
     * enqueue scan jobs for all of them. Replaces any pending queue.
     * Called on root change and on the user's "VCS-Filter aktiv" toggle.
     */
    void schedule_full_rescan(const std::string& root_path);

    /** Number of jobs still pending in the queue. Diagnostic only. */
    std::size_t pending() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace ase::explorer::git

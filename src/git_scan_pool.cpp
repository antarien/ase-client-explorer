/**
 * @file        git_scan_pool.cpp
 * @brief       Implementation of ScanPool
 * @description Owns N worker threads that pull repo paths off a FIFO,
 *              run an in-process libgit2 status scan via the
 *              ase::adp::libgit2 adapter, build per-file FileStatus
 *              entries plus per-directory DirRollup aggregates, and
 *              publish the lot to the StatusCache.
 *
 *              All Repository handles are short-lived (open + scan +
 *              free per job). Reusing handles across jobs is a future
 *              optimisation; the public API does not change.
 *
 *              The job queue is a growable vector with a head index,
 *              guarded by a mutex; the workers idle on a
 *              condition_variable when empty and wake on every
 *              schedule_*() call. Coalescing of duplicate paths is
 *              handled inside enqueue_locked so a storm of
 *              file-watcher events doesn't queue 1000 dupes.
 *
 *              WARUM KEIN RINGPUFFER, obwohl die Regel zu
 *              ase::containers::RingBuffer schickt: dessen Kapazitaet
 *              steht zur Uebersetzungszeit fest (`template<typename T, size_t
 *              Capacity>` in ring_buffer.hpp)
 *              und push() gibt bei Vollstand false zurueck. Hier ist die
 *              Schlange unbegrenzt: bei Vollstand fiele ein Scan-Auftrag
 *              weg, das Abzeichen im Baum bliebe still veraltet, und
 *              diese Einheit koennte es nicht einmal melden — sie bindet
 *              ase::log nicht. Ein Kopfindex auf ase::containers::Vector
 *              bleibt im vorgeschriebenen Modul, verwirft nichts und ist
 *              amortisiert O(1).
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <explorer/git_scan_pool.hpp>
#include <ase/adp/libgit2/repository.hpp>
#include <ase/adp/libgit2/status.hpp>
#include <ase/adp/libgit2/submodule.hpp>

#include <ase/containers/vector.hpp>
#include <ase/math/scalar.hpp>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace ase::explorer::git {

namespace {

// Which DirRollup counter a state feeds, indexed by the FileState value. Clean and
// Ignored carry no counter: they are aggregated nowhere, and one empty slot says that
// once instead of two branches saying it twice.
//
// The static_assert below is load-bearing, not decoration: it is what keeps a newly
// added FileState from silently landing in no counter at all.
constexpr uint32_t DirRollup::* ROLLUP_COUNTER[] = {
    nullptr,                  // Clean
    &DirRollup::modified,     // Modified
    &DirRollup::added,        // Added
    &DirRollup::deleted,      // Deleted
    &DirRollup::renamed,      // Renamed
    &DirRollup::untracked,    // Untracked
    &DirRollup::conflicted,   // Conflicted
    nullptr,                  // Ignored
};

static_assert(sizeof(ROLLUP_COUNTER) / sizeof(ROLLUP_COUNTER[0])
                  == static_cast<size_t>(FileState::Ignored) + 1u,
              "ROLLUP_COUNTER needs exactly one slot per FileState value");

// Map a libgit2 StatusEntry's bit-set to our compact FileState. The
// priority order matches what the tree badge will show: a single file
// with both INDEX_NEW and WT_MODIFIED is "Added" (more interesting
// than a stale modification on a not-yet-staged path).
FileState collapse_state(uint32_t flags) {
    using namespace ase::adp::libgit2;
    if (flags & StatusConflicted) return FileState::Conflicted;
    if (flags & (StatusIndexRenamed | StatusWtRenamed)) return FileState::Renamed;
    if (flags & StatusIndexNew) return FileState::Added;
    if (flags & (StatusIndexDeleted | StatusWtDeleted)) return FileState::Deleted;
    if (flags & (StatusIndexModified | StatusWtModified |
                 StatusIndexTypechange | StatusWtTypechange)) {
        return FileState::Modified;
    }
    if (flags & StatusWtNew) return FileState::Untracked;
    if (flags & StatusIgnored) return FileState::Ignored;
    return FileState::Clean;
}

void bump_rollup(DirRollup& r, FileState s) {
    const size_t index = static_cast<size_t>(s);
    if (index >= sizeof(ROLLUP_COUNTER) / sizeof(ROLLUP_COUNTER[0])) return;

    uint32_t DirRollup::* const counter = ROLLUP_COUNTER[index];
    if (counter == nullptr) return;   // Clean and Ignored are not aggregated

    r.*counter += 1;
}

// Walk every parent directory of `abs_path` up to (but excluding)
// `repo_root`, and bump the rollup map for each ancestor by `state`.
// `repo_root` itself is included so the submodule-row badge picks up
// the same aggregate.
void rollup_ancestors(
    std::unordered_map<std::string, DirRollup>& rollups,
    const std::string& abs_path,
    const std::string& repo_root,
    FileState state)
{
    // Stop at one char before repo_root to include repo_root itself.
    const size_t stop = repo_root.size();
    size_t pos = abs_path.find_last_of('/');
    while (pos != std::string::npos && pos >= stop) {
        bump_rollup(rollups[abs_path.substr(0, pos)], state);
        if (pos == stop) break;            // hit repo_root, done
        pos = abs_path.find_last_of('/', pos - 1);
    }
}

// Recursively enumerate every (sub-)submodule path reachable from
// `parent_root`. Skips paths that fail to open as a repo (a submodule
// that the user never `git submodule init`ed).
void collect_repo_paths(const std::string& parent_root,
                        std::vector<std::string>& out)
{
    out.push_back(parent_root);

    auto opened = ase::adp::libgit2::Repository::open(parent_root);
    if (std::holds_alternative<ase::adp::libgit2::Error>(opened)) return;
    auto& repo = std::get<ase::adp::libgit2::Repository>(opened);

    auto subs = ase::adp::libgit2::submodule::list(repo);
    if (std::holds_alternative<ase::adp::libgit2::Error>(subs)) return;
    const auto& v = std::get<std::vector<ase::adp::libgit2::SubmoduleInfo>>(subs);

    for (const auto& info : v) {
        if (!info.initialized) continue;
        std::string child = parent_root;
        if (!child.empty() && child.back() != '/') child += '/';
        child += info.path;
        collect_repo_paths(child, out);
    }
}

// Run the scan for one repo path, build the publish payload, hand it
// to the StatusCache. Errors are silently swallowed (a single broken
// repo must not poison the rest of the queue).
void scan_and_publish(const std::string& repo_root, StatusCache& cache) {
    auto opened = ase::adp::libgit2::Repository::open(repo_root);
    if (std::holds_alternative<ase::adp::libgit2::Error>(opened)) return;
    auto& repo = std::get<ase::adp::libgit2::Repository>(opened);

    SubmoduleSummary summary;
    summary.head_short = repo.head_short();
    summary.scanned    = true;

    ase::adp::libgit2::StatusOptions opts;
    opts.include_untracked = true;
    opts.include_ignored   = false;
    opts.recurse_untracked = true;

    auto scanned = ase::adp::libgit2::scan(repo, opts);
    if (std::holds_alternative<ase::adp::libgit2::Error>(scanned)) {
        // Publish empty result so the cache stops showing stale data.
        cache.publish_repo(repo_root, summary, {}, {});
        return;
    }
    const auto& result = std::get<ase::adp::libgit2::StatusResult>(scanned);

    std::vector<std::pair<std::string, FileStatus>> file_entries;
    file_entries.reserve(result.entries.size());

    std::unordered_map<std::string, DirRollup> dir_rollups;
    dir_rollups.reserve(result.entries.size());

    for (const auto& e : result.entries) {
        const FileState state = collapse_state(e.flags);
        if (state == FileState::Clean) continue;

        // Build absolute path: repo_root + '/' + e.path (libgit2 paths
        // are repo-relative POSIX). repo_root already has its trailing
        // slash stripped by the libgit2 adapter.
        std::string abs = repo_root;
        if (!abs.empty() && abs.back() != '/') abs += '/';
        abs += e.path;

        FileStatus fs;
        fs.state = state;
        file_entries.emplace_back(std::move(abs), fs);

        // Use the just-built abs path for the rollup walk. We can't
        // reuse the moved-from `abs` so reconstruct it once here.
        std::string abs_for_rollup = repo_root;
        if (!abs_for_rollup.empty() && abs_for_rollup.back() != '/') abs_for_rollup += '/';
        abs_for_rollup += e.path;
        rollup_ancestors(dir_rollups, abs_for_rollup, repo_root, state);
    }

    // Hand the per-repo aggregate to the summary too.
    auto root_rollup_it = dir_rollups.find(repo_root);
    if (root_rollup_it != dir_rollups.end()) {
        summary.rollup = root_rollup_it->second;
    }

    std::vector<std::pair<std::string, DirRollup>> dir_entries;
    dir_entries.reserve(dir_rollups.size());
    for (auto& kv : dir_rollups) {
        dir_entries.emplace_back(std::move(kv.first), std::move(kv.second));
    }

    cache.publish_repo(repo_root, summary,
                       std::move(file_entries), std::move(dir_entries));
}

}  // namespace

struct ScanPool::Impl {
    StatusCache&             cache;
    std::vector<std::thread> workers;
    std::mutex               mu;
    std::condition_variable  cv;

    // FIFO: jobs are appended at the back and consumed from `queue_head` forward. The
    // consumed prefix is dropped once it reaches half the storage, so the walk stays
    // amortised O(1) and the storage cannot creep upward while the queue itself stays
    // short. See the header block for why this is not a ring buffer.
    ase::containers::Vector<std::string> queue;
    std::size_t                          queue_head = 0u;

    std::unordered_set<std::string> queued_paths;  // dedupe set
    std::atomic<bool>        stop_flag{false};
    bool                     started = false;

    explicit Impl(StatusCache& c) : cache(c) {}

    // Caller must hold mu.
    std::size_t pending_locked() const { return queue.size() - queue_head; }

    // Caller must hold mu.
    void enqueue_locked(const std::string& path) {
        if (queued_paths.insert(path).second) {
            queue.push_back(path);
        }
    }

    // Caller must hold mu.
    void clear_locked() {
        queue.clear();
        queue_head = 0u;
    }

    // Caller must hold mu. Only valid when pending_locked() > 0.
    std::string take_front_locked() {
        std::string job = std::move(queue[queue_head]);
        ++queue_head;
        if (queue_head * 2u >= queue.size()) {
            queue.erase(queue.begin(),
                        queue.begin() + static_cast<std::ptrdiff_t>(queue_head));
            queue_head = 0u;
        }
        return job;
    }

    void worker_loop() {
        while (true) {
            std::string job;
            {
                std::unique_lock lk(mu);
                cv.wait(lk, [this] {
                    return stop_flag.load() || pending_locked() > 0u;
                });
                if (stop_flag.load() && pending_locked() == 0u) return;
                job = take_front_locked();
                queued_paths.erase(job);
            }
            scan_and_publish(job, cache);
        }
    }
};

ScanPool::ScanPool(StatusCache& cache)
    : m_impl(std::make_unique<Impl>(cache))
{}

ScanPool::~ScanPool() {
    stop();
}

void ScanPool::start(std::size_t thread_count) {
    if (m_impl->started) return;
    if (thread_count == 0) {
        const unsigned hw = std::thread::hardware_concurrency();
        // Cap at 16 — beyond that the libgit2 file-system stat throughput
        // becomes the bottleneck, and we don't want to starve the GTK
        // main thread on an 8-core box that reports 16 logical cores.
        thread_count = ase::math::min<unsigned>(hw == 0 ? 4 : hw, 16);
    }
    m_impl->stop_flag.store(false);
    m_impl->workers.reserve(thread_count);
    for (std::size_t i = 0; i < thread_count; ++i) {
        m_impl->workers.emplace_back([this] { m_impl->worker_loop(); });
    }
    m_impl->started = true;
}

void ScanPool::stop() {
    if (!m_impl->started) return;
    {
        std::lock_guard lk(m_impl->mu);
        m_impl->stop_flag.store(true);
    }
    m_impl->cv.notify_all();
    for (auto& t : m_impl->workers) {
        if (t.joinable()) t.join();
    }
    m_impl->workers.clear();
    m_impl->started = false;
}

void ScanPool::schedule_repo(const std::string& abs_path) {
    if (abs_path.empty()) return;
    {
        std::lock_guard lk(m_impl->mu);
        m_impl->enqueue_locked(abs_path);
    }
    m_impl->cv.notify_one();
}

void ScanPool::schedule_full_rescan(const std::string& root_path) {
    if (root_path.empty()) return;

    // Enumerate the project tree on the calling thread (typically the
    // GTK main thread). Enumeration touches each .gitmodules + opens
    // each submodule briefly, but the per-repo status scan — the
    // expensive part — runs on workers below.
    std::vector<std::string> repos;
    collect_repo_paths(root_path, repos);

    {
        std::lock_guard lk(m_impl->mu);
        m_impl->clear_locked();
        m_impl->queued_paths.clear();
        for (const auto& p : repos) {
            m_impl->enqueue_locked(p);
        }
    }
    m_impl->cv.notify_all();
}

std::size_t ScanPool::pending() const noexcept {
    std::lock_guard lk(m_impl->mu);
    return m_impl->pending_locked();
}

}  // namespace ase::explorer::git

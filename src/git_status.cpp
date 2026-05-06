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

#include <explorer/icons.hpp>

#include <ase/adp/libcuckoo/map.hpp>

#include <glibmm/main.h>

#include <string>
#include <utility>
#include <vector>

namespace ase::explorer::git {

// Per-repo bookkeeping: which file paths and which (dir_path, rollup)
// pairs the previous publish wrote into the maps. The next publish
// uses this to subtract the prior contribution before adding the new
// one — required because dir_map keys are SHARED with cross-repo
// rollup bumps from nested submodules and a plain insert_or_assign
// would clobber concurrent contributions.
struct OwnedEntry {
    std::vector<std::string> file_paths;
    std::vector<std::pair<std::string, DirRollup>> dir_rollups;
};

struct StatusCache::Impl {
    using FileMap    = ase::adp::libcuckoo::Map<std::string, FileStatus>;
    using DirMap     = ase::adp::libcuckoo::Map<std::string, DirRollup>;
    using SummaryMap = ase::adp::libcuckoo::Map<std::string, SubmoduleSummary>;
    using OwnedMap   = ase::adp::libcuckoo::Map<std::string, OwnedEntry>;

    FileMap    file_map;
    DirMap     dir_map;
    SummaryMap summary_map;
    OwnedMap   owned_map;

    // Project root canonicalised at set_root() time. Cross-repo
    // rollup propagation walks ancestor dirs from a submodule's
    // parent up to (and including) this path, so the explorer's
    // VCS filter does not prune dirs that contain dirty submodules.
    std::string project_root;
};

StatusCache::StatusCache()
    : m_impl(std::make_unique<Impl>())
{}

StatusCache::~StatusCache() = default;

void StatusCache::set_root(const std::string& root_path) {
    clear();
    // Strip a single trailing slash so paths compare equal to libgit2
    // workdir() output. Empty allowed — disables cross-repo propagation
    // entirely until a real root is set.
    std::string canon = root_path;
    if (!canon.empty() && canon.back() == '/') canon.pop_back();
    m_impl->project_root = std::move(canon);
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

namespace {

// Walk every ancestor directory of `start` up to (and including)
// `stop`. `start` and `stop` must be canonical, no trailing slash;
// `stop` must be a prefix of `start` separated by '/'. Calls `fn` for
// each ancestor. When start == stop nothing is called (the submodule
// IS the project root, no cross-repo work to do).
template <typename Fn>
void walk_ancestors_up_to(const std::string& start,
                          const std::string& stop,
                          Fn&& fn)
{
    if (stop.empty() || start.size() <= stop.size()) return;
    if (start.compare(0, stop.size(), stop) != 0) return;

    size_t pos = start.find_last_of('/');
    while (pos != std::string::npos && pos >= stop.size()) {
        fn(start.substr(0, pos));
        if (pos == stop.size()) break;
        pos = start.find_last_of('/', pos - 1);
    }
}

void rollup_add(DirRollup& dst, const DirRollup& src) {
    dst.modified   += src.modified;
    dst.added      += src.added;
    dst.deleted    += src.deleted;
    dst.renamed    += src.renamed;
    dst.untracked  += src.untracked;
    dst.conflicted += src.conflicted;
}

void rollup_sub(DirRollup& dst, const DirRollup& src) {
    // Saturating subtract — guards against underflow if the catalog
    // ever gets out of sync (e.g. a repo replays publish without a
    // prior summary entry surviving a clear()).
    auto sat = [](uint32_t& a, uint32_t b) { a = (a >= b) ? a - b : 0; };
    sat(dst.modified,   src.modified);
    sat(dst.added,      src.added);
    sat(dst.deleted,    src.deleted);
    sat(dst.renamed,    src.renamed);
    sat(dst.untracked,  src.untracked);
    sat(dst.conflicted, src.conflicted);
}

}  // namespace

void StatusCache::publish_repo(
    const std::string& repo_root,
    const SubmoduleSummary& summary,
    std::vector<std::pair<std::string, FileStatus>> file_entries,
    std::vector<std::pair<std::string, DirRollup>>  dir_entries)
{
    // ── 1. Atomically swap out the previous OwnedEntry. The struct
    //       carries every key + rollup we contributed last time so
    //       step 2 can subtract them precisely without disturbing
    //       parallel writers' contributions.
    OwnedEntry previously;
    m_impl->owned_map.update_fn(repo_root,
        [&previously](OwnedEntry& e) {
            previously = std::move(e);
            e = {};
        });

    // ── 2a. Remove the previous file entries. file_map keys are
    //        unique per repo (no submodule paths overlap), so a flat
    //        erase is safe.
    for (const auto& p : previously.file_paths) {
        m_impl->file_map.erase(p);
    }

    // ── 2b. Subtract the previous in-repo dir rollups. dir_map keys
    //        are SHARED with cross-repo bumps from nested submodules,
    //        so we must subtract — never erase. update_fn is atomic;
    //        a parallel publish targeting the same key (e.g. a nested
    //        submodule's cross-repo bump) cannot race.
    for (const auto& [key, old_r] : previously.dir_rollups) {
        m_impl->dir_map.update_fn(key,
            [&old_r](DirRollup& r) { rollup_sub(r, old_r); });
    }

    // ── 2c. Subtract the previous SUMMARY rollup from every cross-
    //        repo ancestor (parent dirs outside this submodule, up
    //        to project root). Skipped on first publish or when this
    //        repo is the project root.
    SubmoduleSummary prev_summary;
    const bool had_prev = m_impl->summary_map.find(repo_root, prev_summary);
    if (had_prev && prev_summary.rollup.any() && !m_impl->project_root.empty()) {
        const DirRollup prev = prev_summary.rollup;
        walk_ancestors_up_to(repo_root, m_impl->project_root,
            [&](const std::string& parent) {
                m_impl->dir_map.update_fn(parent,
                    [&prev](DirRollup& r) { rollup_sub(r, prev); });
            });
    }

    // ── 3a. Add the new file entries.
    OwnedEntry next;
    next.file_paths.reserve(file_entries.size());
    next.dir_rollups.reserve(dir_entries.size());
    for (auto& kv : file_entries) {
        m_impl->file_map.insert_or_assign(kv.first, kv.second);
        next.file_paths.push_back(kv.first);
    }

    // ── 3b. Add the new in-repo dir rollups additively. upsert
    //        handles the "key not present yet" case with a default-
    //        constructed DirRollup, then the lambda runs once to
    //        layer our contribution on top.
    for (auto& kv : dir_entries) {
        const DirRollup add = kv.second;
        m_impl->dir_map.upsert(kv.first,
            [&add](DirRollup& r) { rollup_add(r, add); },
            add);  // initial value if absent
        next.dir_rollups.emplace_back(std::move(kv.first), kv.second);
    }
    m_impl->owned_map.insert_or_assign(repo_root, std::move(next));

    // ── 4. Add the new summary rollup to every cross-repo ancestor.
    if (summary.rollup.any() && !m_impl->project_root.empty()) {
        const DirRollup add = summary.rollup;
        walk_ancestors_up_to(repo_root, m_impl->project_root,
            [&](const std::string& parent) {
                m_impl->dir_map.upsert(parent,
                    [&add](DirRollup& r) { rollup_add(r, add); },
                    add);
            });
    }

    // ── 5. Replace the per-repo summary ───────────────────────────
    m_impl->summary_map.insert_or_assign(repo_root, summary);

    // ── 6. Fire on_updated on the GTK main thread ─────────────────
    auto* sig = &m_on_updated;
    Glib::signal_idle().connect_once([sig]() { sig->emit(); });
}

// ── SSOT colours for VCS file states. Match ase::colors::PANEL_*
//    tokens (sha-web-styles/src/colors.ts → generated/colors.hpp).
//    Single 1-letter glyph per state + same TEXT_FONT + 9pt size as
//    the submodule [L3 feat] badge so both row markers carry equal
//    visual weight.

std::string vcs_file_badge_markup(FileState s) {
    if (s == FileState::Clean || s == FileState::Ignored) return {};

    const char* glyph = "";
    const char* color = "";
    switch (s) {
        case FileState::Modified:   glyph = "M"; color = "#B8863A"; break;  // PANEL_ORANGE
        case FileState::Added:      glyph = "A"; color = "#4A8C6A"; break;  // PANEL_GREEN
        case FileState::Deleted:    glyph = "D"; color = "#A84A4A"; break;  // PANEL_RED
        case FileState::Renamed:    glyph = "R"; color = "#7A5A9C"; break;  // PANEL_PURPLE
        case FileState::Untracked:  glyph = "?"; color = "#5A9CB8"; break;  // PANEL_CYAN
        case FileState::Conflicted: glyph = "U"; color = "#9C8C4A"; break;  // PANEL_YELLOW
        case FileState::Clean:
        case FileState::Ignored:    return {};
    }
    return std::string("<span font_family='") + icons::TEXT_FONT
        + "' font_size='" + std::to_string(9 * 1024)
        + "' foreground='" + color + "'>" + glyph + "</span>";
}

std::string vcs_dir_badge_markup(const DirRollup& r) {
    if (!r.any()) return {};
    auto seg = [](const char* glyph, const char* color, uint32_t n) {
        if (n == 0) return std::string{};
        return std::string("<span font_family='") + icons::TEXT_FONT
             + "' font_size='" + std::to_string(9 * 1024)
             + "' foreground='" + color + "'>" + glyph
             + std::to_string(n) + "</span>";
    };
    std::string out;
    auto append = [&out](std::string s) {
        if (s.empty()) return;
        if (!out.empty()) out += " ";
        out += std::move(s);
    };
    append(seg("M", "#B8863A", r.modified));    // PANEL_ORANGE
    append(seg("A", "#4A8C6A", r.added));       // PANEL_GREEN
    append(seg("D", "#A84A4A", r.deleted));     // PANEL_RED
    append(seg("R", "#7A5A9C", r.renamed));     // PANEL_PURPLE
    append(seg("?", "#5A9CB8", r.untracked));   // PANEL_CYAN
    append(seg("U", "#9C8C4A", r.conflicted));  // PANEL_YELLOW
    return out;
}

}  // namespace ase::explorer::git

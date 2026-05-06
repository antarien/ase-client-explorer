#pragma once

/**
 * @file        breadcrumb.hpp
 * @brief       Path segment bar with clickable ancestors and smart truncation
 * @description Renders the current root path as a sequence of flat buttons.
 *              When the segment count exceeds set_max_segments(), middle
 *              segments collapse into clickable ellipsis (…) buttons. Click
 *              an ellipsis to slide the visible window towards the hidden
 *              range — the bar reshapes itself to keep the total cell count
 *              at exactly `max_segments`, swapping new ellipses in on the
 *              opposite side as needed.
 *
 *              Default max is 5; the explorer pulls a user-configured value
 *              from ExplorerSettings on startup and re-applies it whenever
 *              the user changes the setting in the preferences dialog.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <ase/adp/gtk/widget.hpp>

#include <sigc++/slot.h>

#include <string>
#include <utility>
#include <vector>

namespace ase::explorer::git { class StatusCache; }

namespace ase::explorer {

class Breadcrumb {
public:
    struct Segment {
        std::string label;
        std::string target_path;
    };

    Breadcrumb();

    /** Widget that must be packed into a vertical Box above the tree view. */
    ase::adp::gtk::Box& widget() noexcept { return m_box; }

    /** Install the segment click handler: void(const std::string& path). */
    template <typename Callback>
    void on_segment_clicked(Callback&& callback) {
        m_on_segment_clicked = sigc::slot<void(const std::string&)>(
            [cb = std::forward<Callback>(callback)](const std::string& path) { cb(path); });
    }

    /** Configure the max number of cells (segments + ellipses) in the bar. */
    void set_max_segments(int n);
    int  max_segments() const noexcept { return m_max_segments; }

    /**
     * Set the absolute path under which the breadcrumb starts emitting
     * segments. Anything above this anchor is hidden so the first visible
     * segment is the project root's basename. Window calls this from
     * load_root() with the parent of the current root.
     */
    void set_base(const std::string& absolute_base);

    /** Rebuild the bar for a new absolute path. Resets the focus offset. */
    void update(const std::string& absolute_path);

    /**
     * Install (or clear, with nullptr) the StatusCache the breadcrumb
     * consults to render per-segment VCS aggregates ("M3 ?5"). Cheap
     * pointer assignment — call refresh() afterwards (or wait for the
     * next update()) to repaint with the new cache pointer.
     */
    void set_status_cache(const ase::explorer::git::StatusCache* cache) noexcept {
        m_status_cache = cache;
    }

    /**
     * Re-render the bar for the current path WITHOUT resetting the
     * ellipsis focus offset. Wired to StatusCache::on_updated so the
     * segment aggregates pick up scan results without disturbing the
     * user's scroll position into the path.
     */
    void refresh();

private:
    void render();
    std::vector<Segment> current_segments() const;

    ase::adp::gtk::Box m_box;
    sigc::slot<void(const std::string&)> m_on_segment_clicked;

    std::string m_current_path;
    std::string m_base = "/mnt/code/SRC/GITHUB";  // initial fallback
    int         m_max_segments = 5;
    int         m_focus_offset = 0;  // shifts the visible middle window
    const ase::explorer::git::StatusCache* m_status_cache = nullptr;
};

}  // namespace ase::explorer

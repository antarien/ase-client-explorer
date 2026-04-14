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

namespace ase::explorer {

class Breadcrumb {
public:
    struct Segment {
        std::string label;
        std::string target_path;
    };

    Breadcrumb();

    /** Widget that must be packed into a vertical Box above the tree view. */
    ase::gtk::Box& widget() noexcept { return m_box; }

    /** Install the segment click handler: void(const std::string& path). */
    template <typename Callback>
    void on_segment_clicked(Callback&& callback) {
        m_on_segment_clicked = sigc::slot<void(const std::string&)>(
            [cb = std::forward<Callback>(callback)](const std::string& path) { cb(path); });
    }

    /** Configure the max number of cells (segments + ellipses) in the bar. */
    void set_max_segments(int n);
    int  max_segments() const noexcept { return m_max_segments; }

    /** Rebuild the bar for a new absolute path. Resets the focus offset. */
    void update(const std::string& absolute_path);

private:
    void render();
    std::vector<Segment> current_segments() const;

    ase::gtk::Box m_box;
    sigc::slot<void(const std::string&)> m_on_segment_clicked;

    std::string m_current_path;
    int         m_max_segments = 5;
    int         m_focus_offset = 0;  // shifts the visible middle window
};

}  // namespace ase::explorer

#pragma once

/**
 * @file        breadcrumb.hpp
 * @brief       Path segment bar with clickable ancestors and a copy button
 * @description Renders the current root path as a sequence of flat buttons
 *              separated by "/" labels. Each segment is a parent directory; a
 *              click navigates the window to that directory. A final copy
 *              button copies the absolute path to the clipboard.
 *
 *              The segment click handler is stored as a sigc::slot so the
 *              same callable is reused every time update() rebuilds the bar.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <ase/gtk/widget.hpp>

#include <sigc++/slot.h>

#include <string>
#include <utility>

namespace ase::explorer {

class Breadcrumb {
public:
    Breadcrumb();

    /** Widget that must be packed into a vertical Box above the tree view. */
    ase::gtk::Box& widget() noexcept { return m_box; }

    /** Install the segment click handler: void(const std::string& path). */
    template <typename Callback>
    void on_segment_clicked(Callback&& callback) {
        m_on_segment_clicked = sigc::slot<void(const std::string&)>(
            [cb = std::forward<Callback>(callback)](const std::string& path) { cb(path); });
    }

    /** Rebuild the bar for a new absolute path. */
    void update(const std::string& absolute_path);

private:
    ase::gtk::Box m_box;
    sigc::slot<void(const std::string&)> m_on_segment_clicked;
};

}  // namespace ase::explorer

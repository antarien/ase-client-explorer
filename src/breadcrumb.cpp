/**
 * @file        breadcrumb.cpp
 * @brief       Implementation for breadcrumb.hpp
 * @description Smart-truncating segment bar. The renderer always shows the
 *              first and last segments. The middle (max - 2 cells) is filled
 *              from a sliding window over the hidden segments controlled by
 *              m_focus_offset. Hidden segments on either side become a
 *              clickable "…" button that adjusts the offset and re-renders.
 *
 *              Math: with M total segments and N max cells (N >= 3 enforced
 *              by ExplorerSettings clamp), the default window's left edge is
 *              max(1, M - 1 - (N - 2) - offset). Ellipsis cells consume one
 *              of the N slots on whichever side has hidden segments.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <explorer/breadcrumb.hpp>

#include <ase/adp/gtk/io.hpp>
#include <ase/utils/fs.hpp>

#include <glib.h>

#include <algorithm>
#include <vector>

namespace ase::explorer {

namespace {

// Split an absolute filesystem path into cumulative segments relative to
// the given base. The base is whatever the window passed via set_base()
// — typically the parent directory of the current project root.
std::vector<Breadcrumb::Segment> split_segments_impl(
    const std::string& absolute_path,
    const std::string& base)
{
    std::vector<Breadcrumb::Segment> result;
    if (absolute_path.empty() || base.empty()) return result;

    auto rel = ase::utils::fs::relative_to(absolute_path, base);
    if (rel.empty() || rel == ".") return result;

    std::string accumulator(base);
    std::string buffer;
    for (char c : rel) {
        if (c == '/') {
            if (!buffer.empty()) {
                accumulator += "/";
                accumulator += buffer;
                result.push_back({buffer, accumulator});
                buffer.clear();
            }
        } else {
            buffer.push_back(c);
        }
    }
    if (!buffer.empty()) {
        accumulator += "/";
        accumulator += buffer;
        result.push_back({buffer, accumulator});
    }
    return result;
}

}  // namespace

Breadcrumb::Breadcrumb() : m_box(ase::adp::gtk::Box::horizontal(2)) {
    m_box.set_margin_start(8);
    m_box.set_margin_end(8);
    m_box.set_margin_top(8);
    m_box.set_margin_bottom(8);
}

void Breadcrumb::set_max_segments(int n) {
    const int clamped = std::clamp(n, 3, 30);
    if (clamped == m_max_segments) return;
    m_max_segments = clamped;
    m_focus_offset = 0;
    render();
}

void Breadcrumb::set_base(const std::string& absolute_base) {
    if (absolute_base.empty() || absolute_base == m_base) return;
    m_base = absolute_base;
    m_focus_offset = 0;
    render();
}

void Breadcrumb::update(const std::string& absolute_path) {
    m_current_path = absolute_path;
    m_focus_offset = 0;
    render();
}

std::vector<Breadcrumb::Segment> Breadcrumb::current_segments() const {
    return split_segments_impl(m_current_path, m_base);
}

void Breadcrumb::render() {
    m_box.remove_all_children();

    const auto segments = current_segments();
    const int M = static_cast<int>(segments.size());
    const int N = m_max_segments;

    auto add_segment_button = [&](const Segment& seg) {
        auto btn = ase::adp::gtk::Button::create(seg.label);
        btn.add_css_class("flat");
        btn.add_css_class("dim-label");
        auto target = seg.target_path;
        auto slot = m_on_segment_clicked;
        btn.on_clicked([slot, target]() {
            if (slot) slot(target);
        });
        m_box.append(btn);
    };

    auto add_ellipsis_button = [&](int delta) {
        auto btn = ase::adp::gtk::Button::create("…");
        btn.add_css_class("flat");
        btn.add_css_class("dim-label");
        btn.set_tooltip_text(delta > 0
            ? "Reveal earlier path segments"
            : "Reveal later path segments");
        btn.on_clicked([this, delta]() {
            m_focus_offset += delta;
            if (m_focus_offset < 0) m_focus_offset = 0;
            // CRITICAL: do NOT call render() inline. render() removes every
            // child of m_box including the ellipsis button whose handler is
            // currently executing — that's the GTK_IS_BOX assertion we kept
            // chasing. Defer to the next idle so the click handler returns
            // first and GTK finishes its event dispatch before we tear down
            // the button tree.
            g_idle_add_once(+[](gpointer self) {
                static_cast<Breadcrumb*>(self)->render();
            }, this);
        });
        m_box.append(btn);
    };

    if (M == 0) return;

    if (M <= N) {
        for (const auto& s : segments) add_segment_button(s);
        return;
    }

    // M > N: smart-truncate. Always show first + last; fill the middle
    // (N - 2) cells from a sliding window. Ellipsis cells consume one of
    // those middle slots on whichever side has hidden segments.
    const int middle_cells = N - 2;

    // Default window's left edge sits so the rightmost middle cell is
    // segments[M-2] (segments[M-1] is the always-visible last). Offset
    // shifts the window towards the start of the path.
    int win_start = (M - 1) - middle_cells - m_focus_offset;
    if (win_start < 1) {
        win_start = 1;
        m_focus_offset = (M - 1) - middle_cells - 1;  // clamp into range
        if (m_focus_offset < 0) m_focus_offset = 0;
    }
    int win_end = win_start + middle_cells;  // exclusive
    if (win_end > M - 1) win_end = M - 1;

    const bool left_hidden  = win_start > 1;
    const bool right_hidden = win_end   < M - 1;

    // First segment
    add_segment_button(segments[0]);

    // Left ellipsis consumes the next slot
    int s = win_start;
    int e = win_end;
    if (left_hidden) {
        add_ellipsis_button(+1);  // shift window further left next click
        s += 1;
    }
    if (right_hidden) {
        e -= 1;
    }
    for (int i = s; i < e; ++i) {
        add_segment_button(segments[i]);
    }
    if (right_hidden) {
        add_ellipsis_button(-1);  // shift window back towards the tail
    }

    // Last segment
    add_segment_button(segments[M - 1]);
}

}  // namespace ase::explorer

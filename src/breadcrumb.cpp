/**
 * @file        breadcrumb.cpp
 * @brief       Implementation for breadcrumb.hpp
 * @description update() clears the existing children, splits the path into
 *              segments relative to the ASE SSOT root, creates a flat button
 *              per segment that invokes the stored slot on click, inserts "/"
 *              separator labels between segments, and ends with a copy button
 *              that writes the absolute path to the clipboard.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <explorer/breadcrumb.hpp>

#include <ase/gtk/io.hpp>
#include <ase/utils/fs.hpp>

#include <vector>

namespace ase::explorer {

namespace {

// Anchor for relative path splitting. Matches the single-file explorer: the
// breadcrumb starts at /mnt/code/SRC/GITHUB so "ase/modules/terrain" shows
// three segments ase > modules > terrain.
constexpr const char* BREADCRUMB_BASE = "/mnt/code/SRC/GITHUB";

struct Segment {
    std::string label;       ///< single path component shown as button text
    std::string target_path; ///< absolute path to navigate to when clicked
};

std::vector<Segment> split_segments(const std::string& absolute_path) {
    std::vector<Segment> result;
    auto rel = ase::utils::fs::relative_to(absolute_path, BREADCRUMB_BASE);
    if (rel.empty() || rel == ".") return result;

    std::string accumulator(BREADCRUMB_BASE);
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

Breadcrumb::Breadcrumb() : m_box(ase::gtk::Box::horizontal(2)) {
    m_box.set_margin_start(8);
    m_box.set_margin_end(8);
    m_box.set_margin_top(2);
    m_box.set_margin_bottom(2);
}

void Breadcrumb::update(const std::string& absolute_path) {
    m_box.remove_all_children();

    auto segments = split_segments(absolute_path);
    for (size_t i = 0; i < segments.size(); ++i) {
        if (i > 0) {
            auto sep = ase::gtk::Label::create("/");
            sep.add_css_class("dim-label");
            m_box.append(sep);
        }
        auto btn = ase::gtk::Button::create(segments[i].label);
        btn.add_css_class("flat");
        btn.add_css_class("dim-label");
        auto target = segments[i].target_path;
        auto slot = m_on_segment_clicked;
        btn.on_clicked([slot, target]() {
            if (slot) slot(target);
        });
        m_box.append(btn);
    }

    // Copy/expand actions used to live here as trailing buttons but were
    // moved to the header bar so the breadcrumb row stays a pure path
    // indicator and saves vertical space.
}

}  // namespace ase::explorer

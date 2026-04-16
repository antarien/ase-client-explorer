/**
 * @file        tree_view.cpp
 * @brief       Implementation for tree_view.hpp
 * @description populate() builds a DirectoryList for the root, wraps it in a
 *              FilterListModel that drops excluded entries, and turns the
 *              result into a TreeListModel whose child creator recursively
 *              produces filtered DirectoryLists for each folder. The
 *              SignalListItemFactory uses a side-table of per-row widgets
 *              keyed by the native ListItem pointer to avoid dynamic_cast.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <explorer/tree_view.hpp>

#include <explorer/exclude.hpp>
#include <explorer/icons.hpp>

#include <ase/adp/gtk/io.hpp>
#include <ase/adp/gtk/tree.hpp>
#include <ase/adp/gtk/widget.hpp>
#include <ase/utils/fs.hpp>
#include <ase/utils/strops.hpp>

#include <gtkmm/drawingarea.h>
#include <gtkmm/object.h>
#include <giomm/file.h>
#include <giomm/fileenumerator.h>
#include <giomm/liststore.h>
#include <glibmm/markup.h>
#include <gtk/gtk.h>
#include <cairo.h>

#include <algorithm>

#include <cctype>
#include <fnmatch.h>
#include <new>
#include <string>
#include <vector>

namespace ase::explorer {

namespace {

// Per-row state consumed by the cairo draw callback. Held on the heap with
// a lifetime tied to the DrawingArea via gtk_drawing_area_set_draw_func's
// GDestroyNotify hook. The bind() callback updates the fields in-place and
// calls queue_draw() to trigger a repaint.
struct GuideState {
    unsigned int depth = 0;
    // Size = depth + 1. is_last[0] is self, is_last[i] is the ancestor at
    // tree depth (depth - i). Root row's ancestor is queried against the
    // factory-level root_model by the builder before the vector is filled.
    std::vector<bool> is_last;
};

// Width of one depth column in pixels. Must be in sync with the render loop
// and the DrawingArea size_request in bind().
constexpr int GUIDE_COL_WIDTH = 20;

// X offset within a depth column where the pipe is rendered. The
// Gtk::TreeExpander draws its expand arrow not at the far left of its
// widget but inset by roughly the natural arrow radius (empirically ~10px
// in Adwaita on GTK 4.14+). Placing the pipe at the same x offset makes
// every child's vertical pipe sit directly BELOW the parent row's arrow,
// regardless of GUIDE_COL_WIDTH - the classic Windows-Explorer alignment.
constexpr int GUIDE_ARROW_CENTER_OFFSET = 8;

// Colour of the guide lines (0xAARRGGBB, matches ase::colors::TEXT_DIVIDER).
constexpr double GUIDE_R = 0x3A / 255.0;
constexpr double GUIDE_G = 0x3A / 255.0;
constexpr double GUIDE_B = 0x3A / 255.0;

// Cairo draw function bound via gtk_drawing_area_set_draw_func. Paints the
// tree guide rail for one row: an ancestor-continuation pipe per depth
// column (or blank where the ancestor was a last sibling), and a tee-or-ell
// lead-in at the deepest column pointing at the row content.
void guide_draw_trampoline(GtkDrawingArea* /*area*/, cairo_t* cr,
                            int /*width*/, int height, gpointer user_data)
{
    auto* state = static_cast<GuideState*>(user_data);
    if (!state || state->depth == 0) return;

    cairo_set_source_rgb(cr, GUIDE_R, GUIDE_G, GUIDE_B);
    cairo_set_line_width(cr, 1.0);

    const double h = static_cast<double>(height);
    const double mid_y = h * 0.5;

    for (unsigned int c = 0; c < state->depth; ++c) {
        // Pipe x = column_left + arrow_center_offset. The arrow_center_offset
        // matches the position at which gtk::TreeExpander draws its expand
        // arrow inside its own widget, so every child's pipe is rendered
        // directly underneath the parent row's arrow triangle. +0.5 keeps
        // the 1px cairo stroke aligned to the pixel grid (no antialiased
        // blur).
        const double col_x = c * GUIDE_COL_WIDTH + GUIDE_ARROW_CENTER_OFFSET + 0.5;

        if (c == state->depth - 1) {
            // Deepest column: self lead-in. Always draw a vertical from the
            // top of the row down to the middle, plus a horizontal branch
            // from col_x across the full column width so it reaches the
            // next column's left edge (= the position of this row's own
            // TreeExpander arrow). If the row is NOT the last of its
            // siblings, continue the vertical from the middle down to the
            // bottom so the rail flows into the next sibling row.
            const bool self_last = state->is_last[0];

            cairo_move_to(cr, col_x, 0);
            cairo_line_to(cr, col_x, mid_y);
            cairo_stroke(cr);

            cairo_move_to(cr, col_x, mid_y);
            cairo_line_to(cr, static_cast<double>((c + 1) * GUIDE_COL_WIDTH), mid_y);
            cairo_stroke(cr);

            if (!self_last) {
                cairo_move_to(cr, col_x, mid_y);
                cairo_line_to(cr, col_x, h);
                cairo_stroke(cr);
            }
        } else {
            // Ancestor column: full vertical pipe if that ancestor still has
            // siblings below (i.e., was not the last); blank otherwise.
            // is_last[depth - c - 1] picks the ancestor at tree depth (c + 1).
            const bool ancestor_last = state->is_last[state->depth - c - 1];
            if (!ancestor_last) {
                cairo_move_to(cr, col_x, 0);
                cairo_line_to(cr, col_x, h);
                cairo_stroke(cr);
            }
        }
    }
}

// Heap allocator + destructor used for GuideState so its lifetime is owned
// by the DrawingArea via gtk_drawing_area_set_draw_func's destroy notify.
GuideState* alloc_guide_state() {
    auto* storage = g_malloc0(sizeof(GuideState));
    auto* typed = static_cast<GuideState*>(storage);
    ::new (typed) GuideState();
    return typed;
}

void guide_destroy_notify(gpointer data) {
    auto* state = static_cast<GuideState*>(data);
    if (!state) return;
    state->~GuideState();
    g_free(state);
}

// Side-table value: the widgets inside a row, stored so bind() can update
// them without walking the widget tree with dynamic_cast.
struct RowWidgets {
    Gtk::DrawingArea* guide_area = nullptr;   // row-owned (managed), lives as long as the row widget
    GuideState* guide_state = nullptr;        // lifetime owned by guide_area via destroy notify
    ase::adp::gtk::Label icon_label;
    ase::adp::gtk::Label name_label;
    ase::adp::gtk::Label mapping_dot;
    ase::adp::gtk::Label badge_label;
    ase::adp::gtk::TreeExpander expander;
};

// Per-factory state shared by the setup/bind/teardown lambdas. Lives as long
// as the factory does.
struct FactoryState {
    std::unordered_map<void*, RowWidgets> row_widgets;
    const std::set<std::string>* submodule_paths = nullptr;
    std::unordered_map<std::string, submodule::SubmoduleInfo>* metadata_cache = nullptr;
    // Root ListModel reference used to check is_last_sibling() for depth-0 rows
    // whose get_parent() returns null.
    Glib::RefPtr<Gio::ListModel> root_model;
    // Predicate from ExplorerWindow that says whether a given file extension
    // is currently mapped in FileAssociations. Empty slot disables the dot.
    const sigc::slot<bool(const std::string&)>* is_extension_mapped = nullptr;
};

// Lowercase the basename's extension without leading dot. Empty if the file
// has no extension or starts with a dot (hidden file).
std::string extension_of(const std::string& filename) {
    if (filename.empty() || filename[0] == '.') return {};
    const auto dot = filename.rfind('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 >= filename.size()) return {};
    std::string ext = filename.substr(dot + 1);
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

// Pango markup for the "extension is mapped" indicator. Same NerdFont
// family + same 9pt size as the submodule status badge (build_badge_markup)
// so both indicators have identical visual weight. The glyph is the SSOT
// nf-fa-square (U+F0C8, ICON_SQUARE in ui_icons.hpp) — NOT the raw Unicode
// black square U+25A0 — so it traces back to icon-definitions.ts and the
// icon offset system. The colour is PANEL_PURPLE to stay distinct from
// the submodule palette (cyan/green/orange/yellow).
//
// nf-fa-square is not in iconOffsetExceptions, so the canonical offset
// per icon-replacer.ts is DEFAULT_ICON_OFFSET = { x: -2, y: -1 }. We do
// not apply it here for parity with build_badge_markup, which also omits
// pixel offsets — both labels rely on natural Pango centering inside the
// flowing row layout.
constexpr const char* MAPPING_DOT_MARKUP =
    "<span font_family='FiraCode Nerd Font, JetBrainsMono Nerd Font Mono'"
    " foreground='#7A5A9C' font_size='9216'>\xEF\x83\x88</span>";  // nf-fa-square

// True if `row` is the last of its siblings in its parent row's child model.
// For root-level rows (parent == null), `root_model` is queried instead.
bool is_last_sibling(const Glib::RefPtr<Gtk::TreeListRow>& row,
                     const Glib::RefPtr<Gio::ListModel>& root_model)
{
    if (!row) return false;
    Glib::RefPtr<Gio::ListModel> siblings;
    auto parent = row->get_parent();
    if (parent) {
        siblings = parent->get_children();
    } else {
        siblings = root_model;
    }
    if (!siblings) return false;

    auto my_item = row->get_item();
    if (!my_item) return false;
    const unsigned int n = siblings->get_n_items();
    if (n == 0) return false;

    for (unsigned int i = 0; i < n; ++i) {
        auto s = siblings->get_object(i);
        if (!s) continue;
        // Siblings may contain raw FileInfo items (when child_creator returned
        // a SortListModel on top of a FilterListModel) or wrapped TreeListRow
        // instances (if passthrough=true were used at that level). Handle both.
        auto s_as_row = std::dynamic_pointer_cast<Gtk::TreeListRow>(s);
        auto s_item = s_as_row ? s_as_row->get_item() : s;
        if (s_item.get() == my_item.get()) {
            return i == n - 1;
        }
    }
    return false;
}

// Enumerate a directory synchronously, filter out excluded entries, sort
// (dirs first, alphabetical case insensitive) and return the result as a
// Gio::ListStore<Gio::FileInfo>. Replaces the async Gtk::DirectoryList +
// FilterListModel + SortListModel chain for the explorer tree: a
// synchronous store means set_expanded() immediately exposes the new
// child rows so the recursive-expand button can descend in a single call
// without waiting on GIO async completions on the main loop.
//
// Each FileInfo carries the "standard::file" attribute object (matching
// what Gtk::DirectoryList does internally) so FileInfo::get_full_path()
// can extract the absolute path later in the bind callback.
// Filter match. Two modes:
//   - If `filter` contains glob metacharacters (`*`, `?`, `[`), treat it
//     as an fnmatch pattern (case-insensitive via FNM_CASEFOLD).
//     `*.cpp` matches `window.cpp`, `test_?.txt` matches `test_a.txt`, etc.
//   - Otherwise plain case-insensitive substring match.
//   - Empty needle matches everything.
bool name_matches_filter(const std::string& name, const std::string& filter) {
    if (filter.empty()) return true;

    if (filter.find_first_of("*?[") != std::string::npos) {
        return fnmatch(filter.c_str(), name.c_str(), FNM_CASEFOLD) == 0;
    }

    if (name.size() < filter.size()) return false;
    for (size_t i = 0; i + filter.size() <= name.size(); ++i) {
        bool ok = true;
        for (size_t j = 0; j < filter.size(); ++j) {
            char a = std::tolower(static_cast<unsigned char>(name[i + j]));
            char b = std::tolower(static_cast<unsigned char>(filter[j]));
            if (a != b) { ok = false; break; }
        }
        if (ok) return true;
    }
    return false;
}

// Recursively scan `path` for any file whose name matches the filter.
// Used to prune directories that contain no matches when a filter is
// active — without this, the tree shows a forest of empty subdirectories
// after filtering. Honours exclude::should_exclude so .git, node_modules,
// etc. are skipped during the scan, matching the visible tree exactly.
// Recursion is hand-rolled on top of ase::utils::fs::list_directory so
// that std::filesystem stays confined to the foundation layer.
bool dir_has_filter_match(const std::string& path, const std::string& filter) {
    if (filter.empty()) return true;
    if (!ase::utils::fs::is_directory(path)) return false;

    for (const auto& entry : ase::utils::fs::list_directory(path)) {
        if (exclude::should_exclude(entry.name)) continue;
        if (entry.is_directory) {
            if (dir_has_filter_match(entry.full_path, filter)) return true;
        } else {
            if (name_matches_filter(entry.name, filter)) return true;
        }
    }
    return false;
}

Glib::RefPtr<Gio::ListStore<Gio::FileInfo>> build_sync_dir_store(
    const std::string& path,
    const std::string& filter)
{
    auto store = Gio::ListStore<Gio::FileInfo>::create();
    auto dir_file = Gio::File::create_for_path(path);

    std::vector<Glib::RefPtr<Gio::FileInfo>> items;
    try {
        auto enumerator = dir_file->enumerate_children(
            "standard::name,standard::display-name,standard::type,standard::is-hidden");
        while (auto info = enumerator->next_file()) {
            const auto name = info->get_name();
            if (exclude::should_exclude(name)) continue;

            // Filter logic when a filter is active:
            //  - Files: must match the filter (substring).
            //  - Dirs : must contain at least one matching descendant
            //           file, otherwise we'd show empty branches that the
            //           user has to manually click through. dir_has_filter_match
            //           does a bounded recursive walk via ase::utils::fs.
            const bool is_dir = info->get_file_type() == Gio::FileType::DIRECTORY;
            if (!filter.empty()) {
                if (is_dir) {
                    auto child_path = (ase::utils::fs::Path(path) / name).str();
                    if (!dir_has_filter_match(child_path, filter)) continue;
                } else {
                    if (!name_matches_filter(name, filter)) continue;
                }
            }

            auto child_file = dir_file->get_child(name);
            // Gio::File is an interface, not a concrete GObject subclass in
            // gtkmm's type hierarchy, so the typed set_attribute_object()
            // C++ overload rejects a Glib::RefPtr<Gio::File>. Drop to the
            // C API which takes a plain GObject* pointer.
            g_file_info_set_attribute_object(
                info->gobj(), "standard::file", G_OBJECT(child_file->gobj()));
            items.push_back(info);
        }
        enumerator->close();
    } catch (const Glib::Error&) {
        return store;
    }

    std::sort(items.begin(), items.end(),
        [](const Glib::RefPtr<Gio::FileInfo>& a, const Glib::RefPtr<Gio::FileInfo>& b) {
            const bool a_dir = a->get_file_type() == Gio::FileType::DIRECTORY;
            const bool b_dir = b->get_file_type() == Gio::FileType::DIRECTORY;
            if (a_dir != b_dir) return a_dir;
            std::string la(a->get_name());
            std::string lb(b->get_name());
            for (auto& c : la) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            for (auto& c : lb) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return la < lb;
        });

    for (auto& info : items) {
        store->append(info);
    }
    return store;
}

std::string build_badge_markup(const submodule::SubmoduleInfo& meta) {
    char hex[8];
    ase::utils::format_hex_color(hex, sizeof(hex), submodule::status_color(meta.status));
    // SSOT square: nf-fa-square (U+F0C8 = ICON_SQUARE in ui_icons.hpp). The
    // file-mapping indicator uses the exact same glyph + size so both row
    // markers have identical visual weight; only the colour differs.
    std::string badge = "<span font_family='" + std::string(icons::ICON_FONT) + "' font_size='"
        + std::to_string(9 * 1024) + "' foreground='" + std::string(hex) + "'>\xEF\x83\x88</span>"
        " <span font_family='" + std::string(icons::TEXT_FONT) + "' font_size='"
        + std::to_string(9 * 1024) + "' foreground='#5A5A5A'>[";
    if (meta.layer >= 0) badge += "L" + std::to_string(meta.layer) + " ";
    badge += meta.status + "]</span>";
    return badge;
}

}  // namespace

TreeView::TreeView()
    : m_list_view(std::make_unique<ase::adp::gtk::ListView>(ase::adp::gtk::ListView::create()))
{
    m_list_view->set_vexpand(true);
    m_list_view->set_hexpand(true);
}

void TreeView::populate(const std::string& root_path) {
    // Snapshot expanded directory paths from the previous model so that a
    // refresh (F5 / file-watcher / filter change) doesn't collapse the tree
    // the user is currently working in. Only absolute paths are captured;
    // depth, selection and scroll are intentionally not restored.
    std::set<std::string> previously_expanded;
    if (m_tree_model && m_tree_model->native()) {
        auto native = m_tree_model->native();
        unsigned int pos = 0;
        while (auto row = native->get_row(pos)) {
            if (row->get_expanded()) {
                ase::adp::gtk::TreeListRow tree_row(row);
                auto path = tree_row.get_file_info().get_full_path();
                if (!path.empty()) previously_expanded.insert(std::move(path));
            }
            pos += 1;
        }
    }

    m_current_root = root_path;
    m_submodule_paths = submodule::parse_gitmodules(root_path);
    // Root /VERSION is the SSOT (per WORK_ASE_VERSION_CATALOG.md) — parse it
    // once per populate() and resolve every submodule up front, so bind()
    // only does a map lookup and never touches a per-submodule mirror.
    m_metadata_cache = submodule::parse_root_version(root_path, m_submodule_paths);

    // Build the root list synchronously. The resulting Gio::ListStore is
    // already filtered (exclude list + filename filter) and sorted (dirs
    // first, alphabetical case insensitive), so no FilterListModel /
    // SortListModel is needed on top of it. Synchronous enumeration also
    // means set_expanded() on any descendant row below makes its children
    // visible to subsequent get_row() calls in the same call frame.
    const std::string filter_copy = m_filter;  // captured by value into child_creator
    auto root_store = build_sync_dir_store(root_path, filter_copy);

    // When a filter is active we autoexpand every directory the model
    // creates, so the user immediately sees the matching files inside
    // pruned subdirectories without having to click each parent open.
    const bool autoexpand = !filter_copy.empty();
    auto tree = ase::adp::gtk::TreeListModel::create(
        Glib::RefPtr<Gio::ListModel>(root_store),
        /*passthrough*/ false,
        /*autoexpand*/  autoexpand,
        [filter_copy](ase::adp::gtk::FileInfo& info) -> Glib::RefPtr<Gio::ListModel> {
            if (!info.is_directory()) return {};
            auto child_path = info.get_full_path();
            if (child_path.empty()) return {};
            return build_sync_dir_store(child_path, filter_copy);
        });

    m_tree_model = std::make_unique<ase::adp::gtk::TreeListModel>(tree);
    // MultiSelection gives us Shift-click range selection and Ctrl-click
    // toggle for free. ListView::set_model on the adapter wrapper only
    // accepts SingleSelection so we reach through native() and install the
    // raw selection model ourselves.
    m_selection = Gtk::MultiSelection::create(m_tree_model->native());
    m_list_view->native()->set_model(m_selection);

    // Forward selection-change events to the installed callback so the
    // window can update the breadcrumb bar live as the user clicks around
    // in the tree.
    m_selection->signal_selection_changed().connect(
        [this](guint, guint) {
            if (m_on_selection_changed) {
                m_on_selection_changed(selected_path());
            }
        });

    auto state = std::make_shared<FactoryState>();
    state->submodule_paths = &m_submodule_paths;
    state->metadata_cache = &m_metadata_cache;
    state->root_model = root_store;
    state->is_extension_mapped = &m_is_extension_mapped;

    auto factory = ase::adp::gtk::ListItemFactory::create();

    factory.on_setup([state](ase::adp::gtk::ListItem& item) {
        // Tree guide rail: a Gtk::DrawingArea painted via Cairo in
        // guide_draw_trampoline. The state struct is owned by the drawing
        // area through the destroy notify so freeing the widget frees the
        // state automatically.
        auto* guide_area = Gtk::make_managed<Gtk::DrawingArea>();
        auto* guide_state = alloc_guide_state();
        gtk_drawing_area_set_draw_func(
            GTK_DRAWING_AREA(guide_area->gobj()),
            guide_draw_trampoline,
            guide_state,
            guide_destroy_notify);
        guide_area->set_size_request(0, -1);  // width is updated per row in bind

        auto inner = ase::adp::gtk::Box::horizontal(4);

        auto icon_label = ase::adp::gtk::Label::create("");
        icon_label.set_xalign(0.5f);
        icon_label.set_size_request(22, -1);

        auto name_label = ase::adp::gtk::Label::create("");
        name_label.set_xalign(0.0f);
        name_label.enable_ellipsize_end();
        name_label.set_hexpand(true);
        name_label.set_margin_start(6);

        // Mapping indicator: a small coloured dot painted in the accent
        // colour (PANEL_CYAN) when the file's extension has an explicit
        // FileAssociations entry. Hidden by default — bind() sets the
        // markup per row.
        auto mapping_dot = ase::adp::gtk::Label::create("");
        mapping_dot.set_xalign(0.5f);
        mapping_dot.set_margin_start(4);
        mapping_dot.set_margin_end(4);

        auto badge_label = ase::adp::gtk::Label::create("");
        badge_label.set_xalign(1.0f);

        inner.append(icon_label);
        inner.append(name_label);
        inner.append(mapping_dot);
        inner.append(badge_label);
        // Vertical breathing room was previously applied as listview > row
        // padding, but the drawing area needs to span the full row height
        // (no padding) so adjacent rows' guide rails touch at their seams.
        // Transfer the vertical spacing to the inner content as margin.
        inner.set_margin_top(4);
        inner.set_margin_bottom(4);

        // TreeExpander still handles expand/collapse arrows, but we disable
        // its own depth-indent - we draw the guide rail ourselves in
        // guide_area to get proper corners (├ / └) and ancestor pipes (│).
        auto expander = ase::adp::gtk::TreeExpander::create();
        expander.native()->set_indent_for_depth(false);
        expander.set_child(inner);

        auto outer = ase::adp::gtk::Box::horizontal(0);
        outer.native()->append(*guide_area);
        outer.append(expander);
        item.set_child(outer);

        RowWidgets widgets{guide_area, guide_state, icon_label, name_label,
                           mapping_dot, badge_label, expander};
        state->row_widgets.insert_or_assign(item.native().get(), widgets);
    });

    factory.on_bind([state](ase::adp::gtk::ListItem& item) {
        auto it = state->row_widgets.find(item.native().get());
        if (it == state->row_widgets.end()) return;
        auto& widgets = it->second;

        auto row_obj = item.get_item_native();
        auto row = std::dynamic_pointer_cast<Gtk::TreeListRow>(row_obj);
        if (!row) return;
        ase::adp::gtk::TreeListRow tree_row(row);
        widgets.expander.set_list_row(tree_row);

        // Recompute the per-row guide state: depth + is_last chain walking
        // the parent hierarchy. Then resize the drawing area to exactly
        // `depth` columns wide and queue a repaint.
        const unsigned int depth = row->get_depth();
        widgets.guide_state->depth = depth;
        widgets.guide_state->is_last.clear();
        widgets.guide_state->is_last.reserve(depth + 1);
        {
            auto cur = row;
            while (cur) {
                widgets.guide_state->is_last.push_back(
                    is_last_sibling(cur, state->root_model));
                cur = cur->get_parent();
            }
        }
        widgets.guide_area->set_size_request(
            static_cast<int>(depth) * GUIDE_COL_WIDTH, -1);
        widgets.guide_area->queue_draw();

        auto info = tree_row.get_file_info();
        const std::string name = info.get_name();
        const std::string full_path = info.get_full_path();
        const bool is_dir = info.is_directory();
        const bool is_submodule = state->submodule_paths->count(full_path) > 0;

        // ── Per-row colour overrides (applied to both icon and name) ──
        //
        //  is_docs : any FOLDER whose name contains "docs" (case insensitive),
        //            at any depth → PANEL_PURPLE (#7A5A9C). Catches "docs",
        //            "ase-docs", "user-docs", "Docs", etc.
        //  is_muted: any name starting with '.' or '_' (hidden / private
        //            convention) → TEXT_DIM (#4A4A4A) dark grey, files AND
        //            folders, at any depth.
        //
        // is_docs takes precedence: a folder literally named "_docs" would
        // otherwise be muted, but the docs-marker conveys more semantic
        // information so we keep it visible in purple.
        auto contains_ci_substr = [](const std::string& haystack, const std::string& needle) {
            const size_t nlen = needle.size();
            if (haystack.size() < nlen) return false;
            for (size_t i = 0; i + nlen <= haystack.size(); ++i) {
                bool ok = true;
                for (size_t j = 0; j < nlen; ++j) {
                    char a = std::tolower(static_cast<unsigned char>(haystack[i + j]));
                    char b = std::tolower(static_cast<unsigned char>(needle[j]));
                    if (a != b) { ok = false; break; }
                }
                if (ok) return true;
            }
            return false;
        };

        const bool is_docs  = is_dir && contains_ci_substr(name, std::string("docs"));
        const bool is_muted = !name.empty() && (name[0] == '.' || name[0] == '_');

        if (is_docs) {
            widgets.name_label.set_markup(
                "<span foreground='#7A5A9C'>"
                + Glib::Markup::escape_text(name).raw() + "</span>");
        } else if (is_muted) {
            widgets.name_label.set_markup(
                "<span foreground='#4A4A4A'>"
                + Glib::Markup::escape_text(name).raw() + "</span>");
        } else {
            widgets.name_label.set_text(name);
        }

        icons::ResolvedIcon ri = is_dir
            ? icons::get_folder_icon(tree_row.get_expanded(), is_submodule)
            : icons::get_file_icon(name);
        if (is_docs) {
            ri.r = 0x7A / 255.0;
            ri.g = 0x5A / 255.0;
            ri.b = 0x9C / 255.0;
        } else if (is_muted) {
            ri.r = 0x4A / 255.0;
            ri.g = 0x4A / 255.0;
            ri.b = 0x4A / 255.0;
        }
        widgets.icon_label.set_markup(icons::icon_markup(ri));

        // Mapping indicator: only files, only when the extension is
        // configured in FileAssociations. The check comes from a slot
        // installed by ExplorerWindow so this module stays decoupled.
        if (!is_dir && state->is_extension_mapped && *state->is_extension_mapped) {
            const std::string ext = extension_of(name);
            if (!ext.empty() && (*state->is_extension_mapped)(ext)) {
                widgets.mapping_dot.set_markup(MAPPING_DOT_MARKUP);
                widgets.mapping_dot.set_tooltip_text("." + ext + " is mapped");
            } else {
                widgets.mapping_dot.set_markup("");
                widgets.mapping_dot.set_tooltip_text("");
            }
        } else {
            widgets.mapping_dot.set_markup("");
            widgets.mapping_dot.set_tooltip_text("");
        }

        if (is_submodule) {
            // The cache was pre-filled from root /VERSION in populate() —
            // bind() is a pure lookup here. Absence of a key means the
            // submodule has no authoritative entry yet (e.g. freshly added,
            // `ase version scan` not run) → no badge is drawn.
            auto cache_it = state->metadata_cache->find(full_path);
            if (cache_it != state->metadata_cache->end() && !cache_it->second.status.empty()) {
                const auto& meta = cache_it->second;
                widgets.badge_label.set_markup(build_badge_markup(meta));
                if (!meta.version.empty()) {
                    widgets.badge_label.set_tooltip_text(
                        name + " v" + meta.version + " [" + meta.status + "]");
                }
            } else {
                widgets.badge_label.set_markup("");
                widgets.badge_label.set_tooltip_text("");
            }
        } else {
            widgets.badge_label.set_markup("");
            widgets.badge_label.set_tooltip_text("");
        }
    });

    factory.on_teardown([state](ase::adp::gtk::ListItem& item) {
        state->row_widgets.erase(item.native().get());
    });

    m_factory = std::make_unique<ase::adp::gtk::ListItemFactory>(factory);
    // set_model was already installed via native() right after creating the
    // MultiSelection above; we only need to attach the factory here.
    m_list_view->set_factory(*m_factory);

    // Re-expand every directory that was open in the previous model. Skipped
    // when autoexpand is already on (filter active → TreeListModel expands
    // everything itself). Walks the flattened model forward: because the
    // sync dir store materialises children in-frame, expanding an ancestor
    // inserts its children immediately after the current cursor, so the
    // same linear sweep naturally descends into restored subtrees.
    if (!autoexpand && !previously_expanded.empty() && m_tree_model && m_tree_model->native()) {
        auto native = m_tree_model->native();
        unsigned int pos = 0;
        while (auto row = native->get_row(pos)) {
            if (row->is_expandable() && !row->get_expanded()) {
                ase::adp::gtk::TreeListRow tree_row(row);
                auto path = tree_row.get_file_info().get_full_path();
                if (!path.empty() && previously_expanded.count(path) > 0) {
                    row->set_expanded(true);
                }
            }
            pos += 1;
        }
    }
}

namespace {

// Return the first position (in model order) that is selected in `sel`, or
// GTK_INVALID_LIST_POSITION if nothing is selected. O(n) over the visible
// tree - fast enough for interactive use even on large trees.
guint first_selected_position(const Glib::RefPtr<Gtk::MultiSelection>& sel) {
    if (!sel) return GTK_INVALID_LIST_POSITION;
    const guint n = sel->get_n_items();
    for (guint p = 0; p < n; ++p) {
        if (sel->is_selected(p)) return p;
    }
    return GTK_INVALID_LIST_POSITION;
}

}  // namespace

ase::adp::gtk::FileInfo TreeView::selected_file_info() const {
    const guint pos = first_selected_position(m_selection);
    if (pos == GTK_INVALID_LIST_POSITION) {
        return ase::adp::gtk::FileInfo(Glib::RefPtr<Gio::FileInfo>{});
    }
    auto sel_obj = m_selection->get_object(pos);
    auto row = std::dynamic_pointer_cast<Gtk::TreeListRow>(sel_obj);
    if (!row) return ase::adp::gtk::FileInfo(Glib::RefPtr<Gio::FileInfo>{});
    ase::adp::gtk::TreeListRow tree_row(row);
    return tree_row.get_file_info();
}

std::string TreeView::selected_path() const {
    auto info = selected_file_info();
    if (!info.native()) return std::string{};
    return info.get_full_path();
}

std::vector<std::string> TreeView::selected_paths() const {
    std::vector<std::string> result;
    if (!m_selection) return result;
    const guint n = m_selection->get_n_items();
    for (guint pos = 0; pos < n; ++pos) {
        if (!m_selection->is_selected(pos)) continue;
        auto obj = m_selection->get_object(pos);
        auto row = std::dynamic_pointer_cast<Gtk::TreeListRow>(obj);
        if (!row) continue;
        ase::adp::gtk::TreeListRow tree_row(row);
        auto full = tree_row.get_file_info().get_full_path();
        if (!full.empty()) result.push_back(std::move(full));
    }
    return result;
}

void TreeView::activate_selection() {
    const guint pos = first_selected_position(m_selection);
    if (pos == GTK_INVALID_LIST_POSITION) return;
    auto sel_obj = m_selection->get_object(pos);
    auto row = std::dynamic_pointer_cast<Gtk::TreeListRow>(sel_obj);
    if (!row) return;
    ase::adp::gtk::TreeListRow tree_row(row);
    auto info = tree_row.get_file_info();
    if (info.is_directory()) {
        tree_row.set_expanded(!tree_row.get_expanded());
        return;
    }
    auto full_path = info.get_full_path();
    if (full_path.empty()) return;

    // File activation is delegated to the window so the FileAssociations
    // store can drive the launch. No implicit Gio fallback here — files
    // without an explicit user mapping intentionally do nothing on
    // activation; the user opens them via the context menu's Open With…
    if (m_on_file_activated) m_on_file_activated(full_path);
}

void TreeView::set_filter(const std::string& filter) {
    if (filter == m_filter) return;
    m_filter = filter;
    if (!m_current_root.empty()) populate(m_current_root);
}

void TreeView::toggle_selected_folder() {
    const guint pos = first_selected_position(m_selection);
    if (pos == GTK_INVALID_LIST_POSITION) return;
    auto sel_obj = m_selection->get_object(pos);
    auto row = std::dynamic_pointer_cast<Gtk::TreeListRow>(sel_obj);
    if (!row) return;
    ase::adp::gtk::TreeListRow tree_row(row);
    auto info = tree_row.get_file_info();
    if (!info.is_directory()) return;
    tree_row.set_expanded(!tree_row.get_expanded());
}

bool TreeView::navigate_to(const std::string& target_path) {
    if (target_path.empty() || !m_tree_model || !m_selection || !m_list_view) {
        return false;
    }

    // DFS walk through the flattened view. For every row whose full_path
    // is a proper ancestor of target_path we expand it (safe because the
    // sync dir store materialises children immediately). When we find the
    // row whose full_path matches target_path exactly, we select it,
    // expand it so its own children become visible, and scroll the list
    // view so the row is in view.
    unsigned int pos = 0;
    while (true) {
        auto row = m_tree_model->native()->get_row(pos);
        if (!row) break;

        ase::adp::gtk::TreeListRow tree_row(row);
        auto info = tree_row.get_file_info();
        const std::string row_path = info.get_full_path();

        if (row_path == target_path) {
            // Replace the selection with just this row (second arg "true" =
            // unselect everything else). The navigate-to flow is a jump,
            // not an extension of the user's multi-selection.
            m_selection->select_item(pos, true);
            if (info.is_directory() && !row->get_expanded()) {
                row->set_expanded(true);
            }
            // Scroll the newly-selected row into view. GTK 4.12+ exposes
            // gtk_list_view_scroll_to with a flags enum; FOCUS alone is
            // enough to bring it on screen without fighting the selection
            // state that we already set above.
            gtk_list_view_scroll_to(
                GTK_LIST_VIEW(m_list_view->native()->gobj()),
                pos,
                GTK_LIST_SCROLL_FOCUS,
                nullptr);
            return true;
        }

        // Expand row if it is a strict prefix ancestor of target_path,
        // i.e. target_path starts with row_path + "/". Skip empty
        // row_path (e.g. virtual root) to avoid matching everything.
        if (!row_path.empty() && target_path.size() > row_path.size() &&
            target_path.compare(0, row_path.size(), row_path) == 0 &&
            target_path[row_path.size()] == '/') {
            if (info.is_directory() && !row->get_expanded()) {
                row->set_expanded(true);
            }
        }
        pos += 1;
    }
    return false;
}

bool TreeView::toggle_recursive_expand_selected() {
    if (!m_selection || !m_tree_model) return false;

    // Multi-selection: recursive expand operates on the FIRST selected row
    // (the "anchor"). Users who want to expand many subtrees at once can
    // click each root separately; applying it to every selected row would
    // be surprising and potentially slow.
    const guint selected_pos = first_selected_position(m_selection);
    if (selected_pos == GTK_INVALID_LIST_POSITION) return false;

    auto root_row = m_tree_model->native()->get_row(selected_pos);
    if (!root_row) return false;

    // Only directories can be expanded - silently ignore file selections.
    ase::adp::gtk::TreeListRow root_tree_row(root_row);
    if (!root_tree_row.get_file_info().is_directory()) return false;

    const bool want_expand = !root_row->get_expanded();
    const unsigned int root_depth = root_row->get_depth();

    root_row->set_expanded(want_expand);

    // Collapsing is one-shot: gtk tears the descendants down automatically
    // when the parent flips back to collapsed, so we have no follow-up work.
    if (!want_expand) return true;

    // Expanding deeply: walk forward through the flattened TreeListModel
    // and expand every descendant directory we encounter. Because the
    // flattened view lists rows in depth-first order, advancing pos by one
    // each step naturally descends into the just-exposed first child. When
    // we reach a row whose depth is <= root_depth, we have left the subtree
    // rooted at the initial selection and can stop.
    //
    // Note: set_expanded(true) on an intermediate row inserts new rows
    // right after it without changing our cursor position, so the next
    // iteration picks up the first newly-visible descendant automatically.
    unsigned int pos = selected_pos + 1;
    while (true) {
        auto row = m_tree_model->native()->get_row(pos);
        if (!row) break;
        if (row->get_depth() <= root_depth) break;

        if (row->is_expandable() && !row->get_expanded()) {
            row->set_expanded(true);
        }
        pos += 1;
    }
    return true;
}

}  // namespace ase::explorer

/**
 * @file        main.cpp
 * @brief       ASE Project Explorer - entry point and lifecycle hooks
 * @description Pure orchestrator: creates the ase::gtk::Application, installs
 *              the dark Adwaita color scheme and CSS once at startup, and
 *              hands every activate/open event to a freshly-created
 *              ExplorerWindow. No UI logic, no feature code, no file system
 *              work lives in this file - everything is delegated to the
 *              feature modules under include/explorer and src/.
 *
 * Start modes:
 *   ase-explorer /path/to/project/  : opens the given directory as root
 *   ase-explorer                    : opens the ASE SSOT root
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <explorer/theme.hpp>
#include <explorer/types.hpp>
#include <explorer/window.hpp>

#include <ase/adw/adw.hpp>
#include <ase/gtk/application.hpp>
#include <ase/gtk/style.hpp>
#include <ase/utils/fs.hpp>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

std::unique_ptr<ase::explorer::ExplorerWindow> make_window(ase::gtk::Application& app) {
    auto window = ase::gtk::ApplicationWindow::create(app);
    auto explorer_window = std::make_unique<ase::explorer::ExplorerWindow>(std::move(window));
    explorer_window->build_ui();
    return explorer_window;
}

std::string resolve_initial_path(const std::vector<std::string>& paths) {
    if (paths.empty()) return std::string(ase::explorer::DEFAULT_ROOT);
    const auto& first = paths.front();
    return ase::utils::fs::is_directory(first) ? first : ase::utils::fs::parent_of(first);
}

}  // namespace

int main(int argc, char* argv[]) {
    auto app = ase::gtk::Application::create(
        "com.antarien.ase.explorer",
        ase::gtk::Application::Flags::HandlesOpen);

    // Shared slot holding the currently-presented window so the app keeps it
    // alive for its whole lifetime.
    auto current_window = std::make_shared<std::unique_ptr<ase::explorer::ExplorerWindow>>();

    app.on_startup([]() {
        ase::adw::style_manager::init();
        ase::adw::style_manager::set_color_scheme(ase::adw::ColorScheme::ForceDark);

        auto css = ase::gtk::CssProvider::create();
        css.load_from_data(ase::explorer::theme::generate_css());
        css.install_for_default_display();
    });

    app.on_activate([&app, current_window]() {
        auto win = make_window(app);
        win->load_root(ase::explorer::DEFAULT_ROOT);
        win->present();
        *current_window = std::move(win);
    });

    app.on_open([&app, current_window](const std::vector<std::string>& paths) {
        auto win = make_window(app);
        win->load_root(resolve_initial_path(paths));
        win->present();
        *current_window = std::move(win);
    });

    return app.run(argc, argv);
}

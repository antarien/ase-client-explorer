/**
 * @file        file_watcher.cpp
 * @brief       Implementation for file_watcher.hpp
 * @description start() builds a File handle for the root, constructs a
 *              FileMonitor with the 500ms debounce window, then reconnects
 *              the stored slot as the change handler. stop() drops the
 *              monitor which tears down the underlying Gio::FileMonitor.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <explorer/file_watcher.hpp>

namespace ase::explorer {

void FileWatcher::start(const std::string& root_path) {
    auto file = ase::adp::gtk::File::create_for_path(root_path);
    auto monitor = ase::adp::gtk::FileMonitor::monitor_directory(file, DEBOUNCE_MS);

    auto slot = m_callback;
    monitor.on_changed([slot]() {
        if (slot) slot();
    });

    m_monitor = std::make_unique<ase::adp::gtk::FileMonitor>(monitor);
}

void FileWatcher::stop() {
    m_monitor.reset();
}

}  // namespace ase::explorer

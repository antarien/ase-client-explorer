#pragma once

/**
 * @file        file_watcher.hpp
 * @brief       Debounced directory change notifier
 * @description Wraps ase::adp::gtk::FileMonitor so the window can react to external
 *              file system changes (files added/removed/renamed underneath the
 *              current root) without manually refreshing. Rapid bursts are
 *              coalesced by the adapter's built-in debounce timer.
 *
 *              The callback is stored as a sigc::slot so start() can be called
 *              multiple times (once per root change) without forgetting the
 *              handler.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <ase/adp/gtk/io.hpp>

#include <sigc++/slot.h>

#include <memory>
#include <string>
#include <utility>

namespace ase::explorer {

class FileWatcher {
public:
    /** Debounce window for coalescing bursts of filesystem events (ms). */
    static constexpr int DEBOUNCE_MS = 500;

    FileWatcher() = default;

    /** Install the refresh handler: void(). Called by the window's build_ui. */
    template <typename Callback>
    void on_changed(Callback&& callback) {
        m_callback = sigc::slot<void()>(
            [cb = std::forward<Callback>(callback)]() { cb(); });
    }

    /** (Re-)start watching the given root directory. Replaces any previous monitor. */
    void start(const std::string& root_path);

    /** Stop watching. Safe to call when not started. */
    void stop();

private:
    sigc::slot<void()> m_callback;
    std::unique_ptr<ase::adp::gtk::FileMonitor> m_monitor;
};

}  // namespace ase::explorer

#pragma once

#include "Event.h"
#include <chrono>
#include <memory>
#include <optional>

// Forward declarations
class SharedSimState;
class SimulatorUI;

/**
 * UIUpdateConsumer - Consumes UI updates from the push-based update system.
 *
 * This class is responsible for:
 * 1. Popping updates from the UIUpdateQueue in SharedSimState
 * 2. Tracking update latency for performance monitoring
 * 3. Applying updates to the UI via SimulatorUI
 * 4. Providing metrics about update consumption
 *
 * Thread Safety: This class should only be used from the UI thread.
 */
class UIUpdateConsumer {
public:
    struct Metrics {
        size_t updates_consumed = 0;
        size_t updates_missed = 0; // Updates that were dropped before consumption
        double avg_latency_ms = 0.0;
        double max_latency_ms = 0.0;
        double min_latency_ms = std::numeric_limits<double>::max();
        std::chrono::steady_clock::time_point last_update_time;
    };

    /**
     * Construct a UIUpdateConsumer.
     * @param sim_state Shared simulation state containing the update queue
     * @param ui The UI to apply updates to
     */
    UIUpdateConsumer(SharedSimState* sim_state, SimulatorUI* ui);
    ~UIUpdateConsumer() = default;

    /**
     * Check for and consume any pending UI update.
     * This should be called from the LVGL timer callback at 60fps.
     * @return true if an update was consumed, false otherwise
     */
    bool consumeUpdate();

    /**
     * Get current metrics about update consumption.
     */
    Metrics getMetrics() const { return metrics_; }

    /**
     * Reset metrics to initial state.
     */
    void resetMetrics();

    /**
     * Check if push updates are enabled in SharedSimState.
     */
    bool isPushUpdatesEnabled() const;

private:
    SharedSimState* sim_state_;
    SimulatorUI* ui_;
    Metrics metrics_;

    // Track update sequence for detecting missed updates
    std::optional<uint64_t> last_sequence_num_;

    /**
     * Apply the update to the UI.
     * @param update The update to apply
     */
    void applyUpdate(const UIUpdateEvent& update);

    /**
     * Update latency metrics.
     * @param update The update that was consumed
     */
    void updateLatencyMetrics(const UIUpdateEvent& update);
};
#include "UIUpdateConsumer.h"
#include "SharedSimState.h"
#include "SimulatorUI.h"
#include <spdlog/spdlog.h>

UIUpdateConsumer::UIUpdateConsumer(SharedSimState* sim_state, SimulatorUI* ui)
    : sim_state_(sim_state), ui_(ui), metrics_(), last_sequence_num_(std::nullopt)
{
    if (!sim_state_ || !ui_) {
        throw std::runtime_error(
            "UIUpdateConsumer requires non-null SharedSimState and SimulatorUI");
    }
}

bool UIUpdateConsumer::consumeUpdate()
{
    // Only consume updates if push updates are enabled
    if (!isPushUpdatesEnabled()) {
        return false;
    }

    // Try to pop an update from the queue
    auto update_opt = sim_state_->popUIUpdate();
    if (!update_opt.has_value()) {
        return false;
    }

    const UIUpdateEvent& update = update_opt.value();

    // Track latency
    updateLatencyMetrics(update);

    // Detect missed updates (dropped due to queue overflow)
    if (last_sequence_num_.has_value()) {
        uint64_t expected_next = last_sequence_num_.value() + 1;
        if (update.sequenceNum > expected_next) {
            uint64_t missed = update.sequenceNum - expected_next;
            metrics_.updates_missed += missed;
            spdlog::debug(
                "UIUpdateConsumer: Missed {} updates (seq {} -> {})",
                missed,
                last_sequence_num_.value(),
                update.sequenceNum);
        }
    }
    last_sequence_num_ = update.sequenceNum;

    // Apply the update to the UI
    applyUpdate(update);

    // Update metrics
    metrics_.updates_consumed++;
    metrics_.last_update_time = std::chrono::steady_clock::now();

    return true;
}

void UIUpdateConsumer::resetMetrics()
{
    metrics_ = Metrics();
    last_sequence_num_ = std::nullopt;
}

bool UIUpdateConsumer::isPushUpdatesEnabled() const
{
    return sim_state_->isPushUpdatesEnabled();
}

void UIUpdateConsumer::applyUpdate(const UIUpdateEvent& update)
{
    // Apply the update to the UI
    ui_->applyUpdate(update);

    spdlog::trace(
        "UIUpdateConsumer: Applied update seq={}, fps={}, paused={}",
        update.sequenceNum,
        update.fps,
        update.isPaused);
}

void UIUpdateConsumer::updateLatencyMetrics(const UIUpdateEvent& update)
{
    auto now = std::chrono::steady_clock::now();
    auto update_time = update.timestamp;

    auto latency = std::chrono::duration_cast<std::chrono::microseconds>(now - update_time);
    double latency_ms = latency.count() / 1000.0;

    // Update rolling average
    double alpha = 0.1; // Exponential moving average factor
    if (metrics_.updates_consumed == 0) {
        metrics_.avg_latency_ms = latency_ms;
    }
    else {
        metrics_.avg_latency_ms = (1.0 - alpha) * metrics_.avg_latency_ms + alpha * latency_ms;
    }

    // Update min/max
    metrics_.max_latency_ms = std::max(metrics_.max_latency_ms, latency_ms);
    metrics_.min_latency_ms = std::min(metrics_.min_latency_ms, latency_ms);

    spdlog::trace(
        "UIUpdateConsumer: Update latency: {:.2f}ms (avg: {:.2f}ms)",
        latency_ms,
        metrics_.avg_latency_ms);
}
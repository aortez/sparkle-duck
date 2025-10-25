#pragma once

#include "Event.h"
#include "MaterialType.h"
#include "SimulationStats.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <shared_mutex>

// Forward declaration.
class WorldInterface;

/**
 * @brief Metrics for UI update queue performance monitoring.
 */
struct UIUpdateMetrics {
    uint64_t pushCount; ///< Total updates pushed.
    uint64_t popCount;  ///< Total updates consumed.
    uint64_t dropCount; ///< Updates dropped (overwritten).
};

/**
 * @brief Thread-safe queue for UI updates with latest-update-wins semantics.
 *
 * This queue holds at most one update at a time. When a new update is pushed
 * while one is already pending, the old update is dropped. This ensures the
 * UI always gets the most recent state without building up a backlog.
 */
class UIUpdateQueue {
private:
    mutable std::mutex mutex_;
    std::optional<UIUpdateEvent> latest_;

    // Metrics.
    std::atomic<uint64_t> pushCount_{ 0 };
    std::atomic<uint64_t> popCount_{ 0 };
    std::atomic<uint64_t> dropCount_{ 0 };

public:
    /**
     * @brief Push a new UI update (latest-update-wins).
     * If an update is already pending, it will be replaced.
     */
    void push(UIUpdateEvent update)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (latest_.has_value()) {
            dropCount_++;
        }
        latest_ = std::move(update);
        pushCount_++;
    }

    /**
     * @brief Pop the latest update if available.
     * @return The latest update or empty optional if none pending.
     */
    std::optional<UIUpdateEvent> popLatest()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto result = std::move(latest_);
        if (result.has_value()) {
            popCount_++;
        }
        latest_.reset();
        return result;
    }

    /**
     * @brief Get performance metrics.
     */
    UIUpdateMetrics getMetrics() const
    {
        return { pushCount_.load(), popCount_.load(), dropCount_.load() };
    }

    /**
     * @brief Check if an update is pending.
     */
    bool hasPendingUpdate() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return latest_.has_value();
    }
};

/**
 * @brief Thread-safe shared state for simulation data.
 *
 * This class provides thread-safe access to simulation state that needs
 * to be shared between the UI thread and simulation thread. It uses
 * atomic variables for simple data and mutex protection for complex data.
 */
class SharedSimState {
public:
    SharedSimState() = default;

    // =================================================================
    // ATOMIC STATE (Lock-free access).
    // =================================================================

    /**
     * @brief Check if application should exit.
     */
    bool getShouldExit() const { return shouldExit_.load(std::memory_order_acquire); }

    /**
     * @brief Set exit flag.
     */
    void setShouldExit(bool value) { shouldExit_.store(value, std::memory_order_release); }

    /**
     * @brief Check if simulation is paused.
     */
    bool getIsPaused() const { return isPaused_.load(std::memory_order_acquire); }

    /**
     * @brief Set pause state.
     */
    void setIsPaused(bool value) { isPaused_.store(value, std::memory_order_release); }

    /**
     * @brief Get current simulation step.
     */
    uint32_t getCurrentStep() const { return currentStep_.load(std::memory_order_acquire); }

    /**
     * @brief Set current simulation step.
     */
    void setCurrentStep(uint32_t step) { currentStep_.store(step, std::memory_order_release); }

    /**
     * @brief Get current FPS.
     */
    float getCurrentFPS() const { return currentFPS_.load(std::memory_order_acquire); }

    /**
     * @brief Set current FPS.
     */
    void setCurrentFPS(float fps) { currentFPS_.store(fps, std::memory_order_release); }

    /**
     * @brief Get selected material type.
     */
    MaterialType getSelectedMaterial() const
    {
        return static_cast<MaterialType>(selectedMaterial_.load(std::memory_order_acquire));
    }

    /**
     * @brief Set selected material type.
     */
    void setSelectedMaterial(MaterialType material)
    {
        selectedMaterial_.store(static_cast<int>(material), std::memory_order_release);
    }

    // =================================================================
    // COMPLEX STATE (Mutex-protected).
    // =================================================================

    /**
     * @brief Get simulation statistics (thread-safe copy).
     */
    SimulationStats getStats() const
    {
        std::shared_lock lock(statsMutex_);
        return currentStats_;
    }

    /**
     * @brief Update simulation statistics.
     */
    void updateStats(const SimulationStats& stats)
    {
        std::unique_lock lock(statsMutex_);
        currentStats_ = stats;
    }

    /**
     * @brief Get total mass from stats.
     */
    double getTotalMass() const
    {
        std::shared_lock lock(statsMutex_);
        return currentStats_.totalMass;
    }

    // =================================================================
    // UI STATE PERSISTENCE
    // =================================================================

    /**
     * @brief Physics parameter state for UI persistence.
     */
    struct PhysicsParams {
        double gravity = 9.81;
        double elasticity = 0.8;
        double timescale = 1.0;
        double dynamicStrength = 1.0;
    };

    /**
     * @brief Get physics parameters.
     */
    PhysicsParams getPhysicsParams() const
    {
        std::shared_lock lock(paramsMutex_);
        return physicsParams_;
    }

    /**
     * @brief Update physics parameters.
     */
    void updatePhysicsParams(const PhysicsParams& params)
    {
        std::unique_lock lock(paramsMutex_);
        physicsParams_ = params;
    }

    /**
     * @brief Get current world interface.
     */
    WorldInterface* getCurrentWorld() const
    {
        std::shared_lock lock(worldMutex_);
        return currentWorld_;
    }

    /**
     * @brief Set current world interface.
     */
    void setCurrentWorld(WorldInterface* world)
    {
        std::unique_lock lock(worldMutex_);
        currentWorld_ = world;
    }

    // =================================================================
    // PUSH-BASED UI UPDATE SYSTEM.
    // =================================================================

    /**
     * @brief Check if push-based updates are enabled.
     */
    bool isPushUpdatesEnabled() const { return usePushUpdates_.load(std::memory_order_acquire); }

    /**
     * @brief Enable or disable push-based UI updates.
     * @param enable true to enable push updates, false to use legacy immediate events.
     */
    void enablePushUpdates(bool enable)
    {
        usePushUpdates_.store(enable, std::memory_order_release);
    }

    /**
     * @brief Get next sequence number for UI updates.
     * @return Next monotonic sequence number.
     */
    uint64_t getNextUpdateSequence()
    {
        return updateSequenceNum_.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Push a UI update from simulation thread.
     * Only works if push updates are enabled.
     */
    void pushUIUpdate(UIUpdateEvent update)
    {
        if (isPushUpdatesEnabled()) {
            uiUpdateQueue_.push(std::move(update));
        }
    }

    /**
     * @brief Pop the latest UI update for consumption by UI thread.
     * @return Latest update or empty if none pending.
     */
    std::optional<UIUpdateEvent> popUIUpdate() { return uiUpdateQueue_.popLatest(); }

    /**
     * @brief Get UI update queue metrics for performance monitoring.
     */
    UIUpdateMetrics getUIUpdateMetrics() const { return uiUpdateQueue_.getMetrics(); }

    /**
     * @brief Check if a UI update is pending.
     */
    bool hasUIUpdatePending() const { return uiUpdateQueue_.hasPendingUpdate(); }

private:
    // Atomic variables for lock-free access.
    std::atomic<bool> shouldExit_{ false };
    std::atomic<bool> isPaused_{ false };
    std::atomic<uint32_t> currentStep_{ 0 };
    std::atomic<float> currentFPS_{ 0.0f };
    std::atomic<int> selectedMaterial_{ static_cast<int>(MaterialType::DIRT) };

    // Mutex-protected complex data.
    mutable std::shared_mutex statsMutex_;
    SimulationStats currentStats_;

    mutable std::shared_mutex paramsMutex_;
    PhysicsParams physicsParams_;

    mutable std::shared_mutex worldMutex_;
    WorldInterface* currentWorld_ = nullptr;

    // Push-based UI update system.
    UIUpdateQueue uiUpdateQueue_;
    std::atomic<bool> usePushUpdates_{ false };    // Feature flag - disabled by default.
    std::atomic<uint64_t> updateSequenceNum_{ 0 }; // Monotonic sequence counter.
};
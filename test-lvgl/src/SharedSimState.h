#pragma once

#include "SimulationStats.h"
#include "MaterialType.h"
#include <atomic>
#include <chrono>
#include <shared_mutex>
#include <mutex>

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
    // ATOMIC STATE (Lock-free access)
    // =================================================================
    
    /**
     * @brief Check if application should exit.
     */
    bool getShouldExit() const { 
        return shouldExit_.load(std::memory_order_acquire); 
    }
    
    /**
     * @brief Set exit flag.
     */
    void setShouldExit(bool value) { 
        shouldExit_.store(value, std::memory_order_release); 
    }
    
    /**
     * @brief Check if simulation is paused.
     */
    bool getIsPaused() const { 
        return isPaused_.load(std::memory_order_acquire); 
    }
    
    /**
     * @brief Set pause state.
     */
    void setIsPaused(bool value) { 
        isPaused_.store(value, std::memory_order_release); 
    }
    
    /**
     * @brief Get current simulation step.
     */
    uint32_t getCurrentStep() const { 
        return currentStep_.load(std::memory_order_acquire); 
    }
    
    /**
     * @brief Set current simulation step.
     */
    void setCurrentStep(uint32_t step) { 
        currentStep_.store(step, std::memory_order_release); 
    }
    
    /**
     * @brief Get current FPS.
     */
    float getCurrentFPS() const { 
        return currentFPS_.load(std::memory_order_acquire); 
    }
    
    /**
     * @brief Set current FPS.
     */
    void setCurrentFPS(float fps) { 
        currentFPS_.store(fps, std::memory_order_release); 
    }
    
    /**
     * @brief Get selected material type.
     */
    MaterialType getSelectedMaterial() const {
        return static_cast<MaterialType>(selectedMaterial_.load(std::memory_order_acquire));
    }
    
    /**
     * @brief Set selected material type.
     */
    void setSelectedMaterial(MaterialType material) {
        selectedMaterial_.store(static_cast<int>(material), std::memory_order_release);
    }
    
    // =================================================================
    // COMPLEX STATE (Mutex-protected)
    // =================================================================
    
    /**
     * @brief Get simulation statistics (thread-safe copy).
     */
    SimulationStats getStats() const {
        std::shared_lock lock(statsMutex_);
        return currentStats_;
    }
    
    /**
     * @brief Update simulation statistics.
     */
    void updateStats(const SimulationStats& stats) {
        std::unique_lock lock(statsMutex_);
        currentStats_ = stats;
    }
    
    /**
     * @brief Get total mass from stats.
     */
    double getTotalMass() const {
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
        bool debugEnabled = false;
        bool gravityEnabled = true;
        // Add more as needed
    };
    
    /**
     * @brief Get physics parameters.
     */
    PhysicsParams getPhysicsParams() const {
        std::shared_lock lock(paramsMutex_);
        return physicsParams_;
    }
    
    /**
     * @brief Update physics parameters.
     */
    void updatePhysicsParams(const PhysicsParams& params) {
        std::unique_lock lock(paramsMutex_);
        physicsParams_ = params;
    }
    
private:
    // Atomic variables for lock-free access
    std::atomic<bool> shouldExit_{false};
    std::atomic<bool> isPaused_{false};
    std::atomic<uint32_t> currentStep_{0};
    std::atomic<float> currentFPS_{0.0f};
    std::atomic<int> selectedMaterial_{static_cast<int>(MaterialType::DIRT)};
    
    // Mutex-protected complex data
    mutable std::shared_mutex statsMutex_;
    SimulationStats currentStats_;
    
    mutable std::shared_mutex paramsMutex_;
    PhysicsParams physicsParams_;
};
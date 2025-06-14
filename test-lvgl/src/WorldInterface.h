#pragma once

#include "lvgl/lvgl.h"
#include <cstdint>
#include <memory>

// Forward declarations
class SimulatorUI;

/**
 * \file
 * Abstract interface for world physics systems to enable polymorphic switching
 * between different physics implementations (World/RulesA and WorldB/RulesB).
 * 
 * This interface provides a unified API for the UI and other components while
 * allowing different underlying physics systems and cell types.
 */
class WorldInterface {
public:
    virtual ~WorldInterface() = default;
    
    // =================================================================
    // CORE SIMULATION METHODS
    // =================================================================
    
    // Advance the physics simulation by the given time step
    virtual void advanceTime(double deltaTimeSeconds) = 0;
    
    // Get the current simulation timestep
    virtual uint32_t getTimestep() const = 0;
    
    // Draw the world to the screen
    virtual void draw() = 0;
    
    // Reset the world to initial state
    virtual void reset() = 0;
    
    // =================================================================
    // GRID ACCESS AND PROPERTIES
    // =================================================================
    
    // Get grid dimensions
    virtual uint32_t getWidth() const = 0;
    virtual uint32_t getHeight() const = 0;
    
    // Get the LVGL drawing area object
    virtual lv_obj_t* getDrawArea() const = 0;
    
    // =================================================================
    // SIMULATION CONTROL
    // =================================================================
    
    // Set the simulation time scaling factor
    virtual void setTimescale(double scale) = 0;
    
    // Get total mass of materials in the world
    virtual double getTotalMass() const = 0;
    
    // Get amount of material removed due to threshold cleanup
    virtual double getRemovedMass() const = 0;
    
    // Control whether particles should be added during simulation
    virtual void setAddParticlesEnabled(bool enabled) = 0;
    
    // =================================================================
    // MATERIAL ADDITION
    // =================================================================
    
    // Add dirt material at pixel coordinates
    virtual void addDirtAtPixel(int pixelX, int pixelY) = 0;
    
    // Add water material at pixel coordinates  
    virtual void addWaterAtPixel(int pixelX, int pixelY) = 0;
    
    // =================================================================
    // DRAG INTERACTION
    // =================================================================
    
    // Start dragging material from pixel coordinates
    virtual void startDragging(int pixelX, int pixelY) = 0;
    
    // Update drag position
    virtual void updateDrag(int pixelX, int pixelY) = 0;
    
    // End dragging and place material
    virtual void endDragging(int pixelX, int pixelY) = 0;
    
    // Restore the last dragged cell to its original state
    virtual void restoreLastDragCell() = 0;
    
    // =================================================================
    // PHYSICS PARAMETERS
    // =================================================================
    
    // Set gravity strength
    virtual void setGravity(double g) = 0;
    
    // Set elasticity factor for collisions (0.0 to 1.0)
    virtual void setElasticityFactor(double e) = 0;
    
    // Set pressure system scaling factor
    virtual void setPressureScale(double scale) = 0;
    
    // Set dirt fragmentation factor during transfers
    virtual void setDirtFragmentationFactor(double factor) = 0;
    
    // =================================================================
    // WATER PHYSICS PARAMETERS
    // =================================================================
    
    // Set threshold for water pressure application
    virtual void setWaterPressureThreshold(double threshold) = 0;
    
    // Get current water pressure threshold
    virtual double getWaterPressureThreshold() const = 0;
    
    // =================================================================
    // PRESSURE SYSTEM SELECTION
    // =================================================================
    
    // Pressure system variants (some implementations may ignore)
    enum class PressureSystem {
        Original,         // COM deflection based pressure
        TopDown,          // Hydrostatic accumulation top-down
        IterativeSettling // Multiple settling passes
    };
    
    // Set the pressure system algorithm
    virtual void setPressureSystem(PressureSystem system) = 0;
    
    // Get current pressure system
    virtual PressureSystem getPressureSystem() const = 0;
    
    // =================================================================
    // TIME REVERSAL FUNCTIONALITY
    // =================================================================
    
    // Enable or disable time reversal capability
    virtual void enableTimeReversal(bool enabled) = 0;
    
    // Check if time reversal is enabled
    virtual bool isTimeReversalEnabled() const = 0;
    
    // Save current world state for time reversal
    virtual void saveWorldState() = 0;
    
    // Check if we can go backward in time
    virtual bool canGoBackward() const = 0;
    
    // Check if we can go forward in time
    virtual bool canGoForward() const = 0;
    
    // Go backward one step in time
    virtual void goBackward() = 0;
    
    // Go forward one step in time
    virtual void goForward() = 0;
    
    // Clear all time reversal history
    virtual void clearHistory() = 0;
    
    // Get number of saved states in history
    virtual size_t getHistorySize() const = 0;
    
    // =================================================================
    // WORLD SETUP CONTROLS
    // =================================================================
    
    // Enable/disable left-side particle throwing
    virtual void setLeftThrowEnabled(bool enabled) = 0;
    
    // Enable/disable right-side particle throwing
    virtual void setRightThrowEnabled(bool enabled) = 0;
    
    // Enable/disable lower-right quadrant features
    virtual void setLowerRightQuadrantEnabled(bool enabled) = 0;
    
    // Enable/disable world boundary walls
    virtual void setWallsEnabled(bool enabled) = 0;
    
    // Set rain particle generation rate
    virtual void setRainRate(double rate) = 0;
    
    // Check if left throw is enabled
    virtual bool isLeftThrowEnabled() const = 0;
    
    // Check if right throw is enabled
    virtual bool isRightThrowEnabled() const = 0;
    
    // Check if lower-right quadrant is enabled
    virtual bool isLowerRightQuadrantEnabled() const = 0;
    
    // Check if walls are enabled
    virtual bool areWallsEnabled() const = 0;
    
    // Get current rain rate
    virtual double getRainRate() const = 0;
    
    // =================================================================
    // CURSOR FORCE INTERACTION
    // =================================================================
    
    // Enable/disable cursor force interaction
    virtual void setCursorForceEnabled(bool enabled) = 0;
    
    // Update cursor force at pixel coordinates
    virtual void updateCursorForce(int pixelX, int pixelY, bool isActive) = 0;
    
    // Clear cursor force effect
    virtual void clearCursorForce() = 0;
    
    // =================================================================
    // GRID MANAGEMENT
    // =================================================================
    
    // Resize the simulation grid
    virtual void resizeGrid(uint32_t newWidth, uint32_t newHeight, bool clearHistory = true) = 0;
    
    // =================================================================
    // PERFORMANCE AND DEBUGGING
    // =================================================================
    
    // Dump performance timer statistics
    virtual void dumpTimerStats() const = 0;
    
    // Mark that user input has occurred (for state saving triggers)
    virtual void markUserInput() = 0;
    
    // =================================================================
    // UI INTEGRATION
    // =================================================================
    
    // Set the UI component (for bidirectional communication)
    virtual void setUI(std::unique_ptr<SimulatorUI> ui) = 0;
    
    // Get the UI component
    virtual SimulatorUI* getUI() const = 0;
};
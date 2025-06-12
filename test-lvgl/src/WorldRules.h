#pragma once

#include "Cell.h"
#include "Vector2d.h"

#include <cstdint>
#include <memory>
#include <string>

// Forward declarations
class World;

/**
 * Abstract base class defining physics rules for the World simulation.
 * This allows different rule sets to be swapped in and out, enabling
 * experimentation with different physics behaviors.
 */
class WorldRules {
public:
    virtual ~WorldRules() = default;

    // Core physics methods that must be implemented by concrete rules
    virtual void applyPhysics(
        Cell& cell, uint32_t x, uint32_t y, double deltaTimeSeconds, const World& world) = 0;
    virtual void updatePressures(World& world, double deltaTimeSeconds) = 0;
    virtual void applyPressureForces(World& world, double deltaTimeSeconds) = 0;

    // Transfer and collision mechanics
    virtual bool shouldTransfer(
        const Cell& cell, uint32_t x, uint32_t y, const World& world) const = 0;
    virtual void calculateTransferDirection(
        const Cell& cell,
        bool& shouldTransferX,
        bool& shouldTransferY,
        int& targetX,
        int& targetY,
        Vector2d& comOffset,
        uint32_t x,
        uint32_t y,
        const World& world) const = 0;
    virtual bool attemptTransfer(
        Cell& cell,
        uint32_t x,
        uint32_t y,
        int targetX,
        int targetY,
        const Vector2d& comOffset,
        double totalMass,
        World& world) = 0;
    virtual void handleTransferFailure(
        Cell& cell,
        uint32_t x,
        uint32_t y,
        int targetX,
        int targetY,
        bool shouldTransferX,
        bool shouldTransferY,
        World& world) = 0;
    virtual void handleBoundaryReflection(
        Cell& cell, int targetX, int targetY, bool shouldTransferX, bool shouldTransferY, World& world) = 0;
    virtual void checkExcessiveDeflectionReflection(Cell& cell, World& world) = 0;
    virtual void handleCollision(
        Cell& cell,
        uint32_t x,
        uint32_t y,
        int targetX,
        int targetY,
        bool shouldTransferX,
        bool shouldTransferY,
        const World& world) = 0;

    // Material properties and constants
    virtual double getGravity() const = 0;
    virtual double getElasticityFactor() const = 0;
    virtual double getPressureScale() const = 0;
    virtual double getWaterPressureThreshold() const = 0;
    virtual double getDirtFragmentationFactor() const = 0;

    // Rule identification
    virtual std::string getName() const = 0;
    virtual std::string getDescription() const = 0;

    // Configuration interface
    virtual void setGravity(double gravity) = 0;
    virtual void setElasticityFactor(double factor) = 0;
    virtual void setPressureScale(double scale) = 0;
    virtual void setWaterPressureThreshold(double threshold) = 0;
    virtual void setDirtFragmentationFactor(double factor) = 0;

    // Helper methods available to all rules implementations
    static bool isWithinBounds(int x, int y, const World& world);
    static Vector2d calculateNaturalCOM(const Vector2d& sourceCOM, int deltaX, int deltaY);
    static Vector2d clampCOMToDeadZone(const Vector2d& naturalCOM);

protected:
};

/**
 * RulesA physics implementation that matches the current World behavior.
 * This preserves the existing behavior while enabling future rule variations.
 */
class RulesA : public WorldRules {
public:
    RulesA();

    // Core physics implementation
    void applyPhysics(
        Cell& cell, uint32_t x, uint32_t y, double deltaTimeSeconds, const World& world) override;
    void updatePressures(World& world, double deltaTimeSeconds) override;
    void applyPressureForces(World& world, double deltaTimeSeconds) override;

    // Transfer and collision mechanics
    bool shouldTransfer(
        const Cell& cell, uint32_t x, uint32_t y, const World& world) const override;
    void calculateTransferDirection(
        const Cell& cell,
        bool& shouldTransferX,
        bool& shouldTransferY,
        int& targetX,
        int& targetY,
        Vector2d& comOffset,
        uint32_t x,
        uint32_t y,
        const World& world) const override;
    bool attemptTransfer(
        Cell& cell,
        uint32_t x,
        uint32_t y,
        int targetX,
        int targetY,
        const Vector2d& comOffset,
        double totalMass,
        World& world) override;
    void handleTransferFailure(
        Cell& cell,
        uint32_t x,
        uint32_t y,
        int targetX,
        int targetY,
        bool shouldTransferX,
        bool shouldTransferY,
        World& world) override;
    void handleBoundaryReflection(
        Cell& cell, int targetX, int targetY, bool shouldTransferX, bool shouldTransferY, World& world) override;
    void checkExcessiveDeflectionReflection(Cell& cell, World& world) override;
    void handleCollision(
        Cell& cell,
        uint32_t x,
        uint32_t y,
        int targetX,
        int targetY,
        bool shouldTransferX,
        bool shouldTransferY,
        const World& world) override;

    // Material properties
    double getGravity() const override { return gravity_; }
    double getElasticityFactor() const override { return elasticityFactor_; }
    double getPressureScale() const override { return pressureScale_; }
    double getWaterPressureThreshold() const override { return waterPressureThreshold_; }
    double getDirtFragmentationFactor() const override { return dirtFragmentationFactor_; }

    // Rule identification
    std::string getName() const override { return "RulesA"; }
    std::string getDescription() const override
    {
        return "RulesA physics with COM-based pressure system";
    }

    // Configuration
    void setGravity(double gravity) override { gravity_ = gravity; }
    void setElasticityFactor(double factor) override { elasticityFactor_ = factor; }
    void setPressureScale(double scale) override { pressureScale_ = scale; }
    void setWaterPressureThreshold(double threshold) override
    {
        waterPressureThreshold_ = threshold;
    }
    void setDirtFragmentationFactor(double factor) override { dirtFragmentationFactor_ = factor; }


private:
    // Physics constants
    double gravity_ = 9.81;
    double elasticityFactor_ = 0.8;
    double pressureScale_ = 1.0;
    double waterPressureThreshold_ = 0.0004;
    double dirtFragmentationFactor_ = 0.0;


    // Cursor force state (could be moved to World if shared across rules)
    static constexpr double CURSOR_FORCE_STRENGTH = 10.0;
    static constexpr double CURSOR_FORCE_RADIUS = 5.0;
};


/**
 * Factory function to create different rule sets
 */
std::unique_ptr<WorldRules> createWorldRules(const std::string& rulesType);
#pragma once

#include "CellB.h"
#include "Vector2d.h"

#include <cstdint>
#include <string>

// Forward declarations
class WorldB;

/**
 * Interface for physics rules that work with WorldB and CellB
 * This is separate from WorldRules to avoid breaking existing code
 */
class WorldRulesBInterface {
public:
    virtual ~WorldRulesBInterface() = default;

    // Core physics methods for CellB/WorldB
    virtual void applyPhysics(
        CellB& cell, uint32_t x, uint32_t y, double deltaTimeSeconds, const WorldB& world) = 0;
    virtual void updatePressures(WorldB& world, double deltaTimeSeconds) = 0;
    virtual void applyPressureForces(WorldB& world, double deltaTimeSeconds) = 0;

    // Transfer and collision mechanics for CellB
    virtual bool shouldTransfer(
        const CellB& cell, uint32_t x, uint32_t y, const WorldB& world) const = 0;
    virtual void calculateTransferDirection(
        const CellB& cell,
        bool& shouldTransferX,
        bool& shouldTransferY,
        int& targetX,
        int& targetY,
        Vector2d& comOffset,
        uint32_t x,
        uint32_t y,
        const WorldB& world) const = 0;
    virtual bool attemptTransfer(
        CellB& cell,
        uint32_t x,
        uint32_t y,
        int targetX,
        int targetY,
        const Vector2d& comOffset,
        double totalMass,
        WorldB& world) = 0;
    virtual void handleTransferFailure(
        CellB& cell,
        uint32_t x,
        uint32_t y,
        int targetX,
        int targetY,
        bool shouldTransferX,
        bool shouldTransferY,
        WorldB& world) = 0;
    virtual void handleBoundaryReflection(
        CellB& cell,
        int targetX,
        int targetY,
        bool shouldTransferX,
        bool shouldTransferY,
        WorldB& world) = 0;
    virtual void checkExcessiveDeflectionReflection(CellB& cell, WorldB& world) = 0;
    virtual void handleCollision(
        CellB& cell,
        uint32_t x,
        uint32_t y,
        int targetX,
        int targetY,
        bool shouldTransferX,
        bool shouldTransferY,
        const WorldB& world) = 0;

    // Material properties
    virtual double getGravity() const = 0;
    virtual double getElasticityFactor() const = 0;
    virtual double getPressureScale() const = 0;
    virtual double getWaterPressureThreshold() const = 0;
    virtual double getDirtFragmentationFactor() const = 0;

    virtual std::string getName() const = 0;
    virtual std::string getDescription() const = 0;

    // Configuration
    virtual void setGravity(double gravity) = 0;
    virtual void setElasticityFactor(double factor) = 0;
    virtual void setPressureScale(double scale) = 0;
    virtual void setWaterPressureThreshold(double threshold) = 0;
    virtual void setDirtFragmentationFactor(double factor) = 0;

protected:
    // Helper methods that can be shared between implementations
    bool isWithinBounds(int x, int y, const WorldB& world) const;
};
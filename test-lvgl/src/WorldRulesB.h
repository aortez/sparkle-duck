#pragma once

#include "WorldRules.h"

class RulesB : public WorldRules {
public:
    RulesB();

    void applyPhysics(Cell& cell, uint32_t x, uint32_t y, double deltaTimeSeconds, const World& world) override;
    void updatePressures(World& world, double deltaTimeSeconds) override;
    void applyPressureForces(World& world, double deltaTimeSeconds) override;
    bool shouldTransfer(const Cell& cell, uint32_t x, uint32_t y, const World& world) const override;
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
        Cell& cell,
        int targetX,
        int targetY,
        bool shouldTransferX,
        bool shouldTransferY,
        World& world) override;
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

    std::string getName() const override { return "World Rules B"; }
    std::string getDescription() const override
    {
        return "Experimental physics rules for testing alternative behaviors";
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
    // Physics constants (could be different from RulesA)
    double gravity_ = 9.81;
    double elasticityFactor_ = 0.8;
    double pressureScale_ = 1.0;
    double waterPressureThreshold_ = 0.0004;
    double dirtFragmentationFactor_ = 0.0;
};
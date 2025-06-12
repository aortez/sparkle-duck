#pragma once

#include "WorldRulesBInterface.h"

class RulesBNew : public WorldRulesBInterface {
public:
    RulesBNew();

    void applyPhysics(CellB& cell, uint32_t x, uint32_t y, double deltaTimeSeconds, const WorldB& world) override;
    void updatePressures(WorldB& world, double deltaTimeSeconds) override;
    void applyPressureForces(WorldB& world, double deltaTimeSeconds) override;
    bool shouldTransfer(const CellB& cell, uint32_t x, uint32_t y, const WorldB& world) const override;
    void calculateTransferDirection(
        const CellB& cell,
        bool& shouldTransferX,
        bool& shouldTransferY,
        int& targetX,
        int& targetY,
        Vector2d& comOffset,
        uint32_t x,
        uint32_t y,
        const WorldB& world) const override;
    bool attemptTransfer(
        CellB& cell,
        uint32_t x,
        uint32_t y,
        int targetX,
        int targetY,
        const Vector2d& comOffset,
        double totalMass,
        WorldB& world) override;
    void handleTransferFailure(
        CellB& cell,
        uint32_t x,
        uint32_t y,
        int targetX,
        int targetY,
        bool shouldTransferX,
        bool shouldTransferY,
        WorldB& world) override;
    void handleBoundaryReflection(
        CellB& cell,
        int targetX,
        int targetY,
        bool shouldTransferX,
        bool shouldTransferY,
        WorldB& world) override;
    void checkExcessiveDeflectionReflection(CellB& cell, WorldB& world) override;
    void handleCollision(
        CellB& cell,
        uint32_t x,
        uint32_t y,
        int targetX,
        int targetY,
        bool shouldTransferX,
        bool shouldTransferY,
        const WorldB& world) override;

    // Material properties
    double getGravity() const override { return gravity_; }
    double getElasticityFactor() const override { return elasticityFactor_; }
    double getPressureScale() const override { return pressureScale_; }
    double getWaterPressureThreshold() const override { return waterPressureThreshold_; }
    double getDirtFragmentationFactor() const override { return dirtFragmentationFactor_; }

    std::string getName() const override { return "World Rules B (New)"; }
    std::string getDescription() const override
    {
        return "New physics rules implementing cell fill ratios and pure materials";
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

    // Helper method to calculate natural COM offset
    Vector2d calculateNaturalCOM(const Vector2d& currentCom, int deltaX, int deltaY) const;
    Vector2d clampCOMToDeadZone(const Vector2d& com) const;
};
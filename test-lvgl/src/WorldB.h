#pragma once
/**
 * \file
 * WorldB - A simplified world implementation for WorldRulesB 
 * Uses CellB with fill_ratio and pure materials instead of Cell with dirt/water
 */

#include "CellB.h"
#include "Timers.h"

#include <cstdint>
#include <memory>
#include <vector>

// Forward declarations
class WorldRulesBInterface;

struct MaterialMove {
    uint32_t fromX;
    uint32_t fromY;
    uint32_t toX;
    uint32_t toY;
    double amount;
    MaterialType materialType;
    Vector2d comOffset;
};

class WorldB {
    friend class RulesB;
    friend class RulesBNew;
public:
    WorldB(uint32_t width, uint32_t height);
    ~WorldB();

    // WorldB is not copyable
    WorldB(const WorldB&) = delete;
    WorldB& operator=(const WorldB&) = delete;

    // Move constructor and assignment
    WorldB(WorldB&&) = default;
    WorldB& operator=(WorldB&&) = default;

    // Grid access
    uint32_t getWidth() const { return width; }
    uint32_t getHeight() const { return height; }
    
    CellB& at(uint32_t x, uint32_t y) { return cells[coordToIndex(x, y)]; }
    const CellB& at(uint32_t x, uint32_t y) const { return cells[coordToIndex(x, y)]; }

    // Physics simulation
    void advanceTime(double deltaTimeSeconds);

    // Set physics rules (keeping old interface for compatibility - not actually used)
    // void setWorldRules(std::unique_ptr<WorldRules> rules) { worldRules_ = std::move(rules); }
    // WorldRules* getWorldRules() const { return worldRules_.get(); }
    
    // Set new physics rules
    void setWorldRulesBNew(std::unique_ptr<WorldRulesBInterface> rules) { worldRulesBNew_ = std::move(rules); }
    WorldRulesBInterface* getWorldRulesBNew() const { return worldRulesBNew_.get(); }
    
    // Add move to queue (for rules to use)
    void addMove(const MaterialMove& move) { moves.push_back(move); }

    // Physics constants
    static constexpr double MIN_FILL_THRESHOLD = 0.01;
    static constexpr double COM_CELL_WIDTH = 2.0;
    static constexpr double REFLECTION_THRESHOLD = 1.2;
    static constexpr double TRANSFER_FACTOR = 1.0;

    // Get current timestep
    uint32_t getTimestep() const { return timestep; }

    // Initialize world with some materials for testing
    void initializeTestMaterials();

    // Validate world state
    void validateState(const std::string& context) const;

private:
    uint32_t width;
    uint32_t height;
    std::vector<CellB> cells;
    uint32_t timestep = 0;

    Timers timers;

    // Physics rules interface
    std::unique_ptr<WorldRulesBInterface> worldRulesBNew_;

    // Move queue for transfers
    std::vector<MaterialMove> moves;

    // Helper methods
    size_t coordToIndex(uint32_t x, uint32_t y) const { return y * width + x; }
    
    void processTransfers(double deltaTimeSeconds);
    void applyMoves();
};
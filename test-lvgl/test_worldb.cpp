#include "src/WorldB.h"
#include "src/RulesBNew.h"
#include "spdlog/spdlog.h"

#include <iostream>
#include <memory>

int main() {
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("Testing WorldB with RulesBNew");

    // Create a small world
    auto world = std::make_unique<WorldB>(20, 20);
    auto rules = std::make_unique<RulesBNew>();
    
    world->setWorldRulesBNew(std::move(rules));
    
    // Initialize with test materials
    world->initializeTestMaterials();
    
    // Print initial state
    spdlog::info("Initial state:");
    for (uint32_t y = 0; y < 5; ++y) {
        std::string row;
        for (uint32_t x = 0; x < 20; ++x) {
            const CellB& cell = world->at(x, y);
            char symbol = ' ';
            switch (cell.material) {
                case MaterialType::AIR: symbol = '.'; break;
                case MaterialType::DIRT: symbol = 'D'; break;
                case MaterialType::WATER: symbol = 'W'; break;
                case MaterialType::WOOD: symbol = '#'; break;
                case MaterialType::WALL: symbol = '|'; break;
                default: symbol = '?'; break;
            }
            row += symbol;
        }
        spdlog::info("Row {}: {}", y, row);
    }
    
    // Run a few simulation steps
    for (int i = 0; i < 5; ++i) {
        world->advanceTime(0.016); // ~60 FPS
        spdlog::debug("Completed timestep {}", i + 1);
    }
    
    // Validate world state
    world->validateState("After simulation");
    
    spdlog::info("WorldB test completed successfully!");
    return 0;
}
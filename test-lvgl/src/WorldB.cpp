#include "WorldB.h"
#include "WorldRulesBInterface.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <random>

WorldB::WorldB(uint32_t width, uint32_t height) 
    : width(width), height(height), cells(width * height)
{
    spdlog::info("Created WorldB with dimensions {}x{}", width, height);
    
    // Initialize all cells as empty air
    for (auto& cell : cells) {
        cell.setMaterial(MaterialType::AIR, 0.0);
    }
    
    // Set boundary cells to walls
    for (uint32_t x = 0; x < width; ++x) {
        at(x, 0).setMaterial(MaterialType::WALL, 1.0);           // Top wall
        at(x, height - 1).setMaterial(MaterialType::WALL, 1.0);  // Bottom wall
    }
    for (uint32_t y = 0; y < height; ++y) {
        at(0, y).setMaterial(MaterialType::WALL, 1.0);           // Left wall
        at(width - 1, y).setMaterial(MaterialType::WALL, 1.0);   // Right wall
    }
    
    spdlog::debug("WorldB initialization complete with boundary walls");
}

WorldB::~WorldB() = default;

void WorldB::advanceTime(double deltaTimeSeconds)
{
    // Use new interface if available, otherwise fall back to old one
    if (worldRulesBNew_) {
        timers.startTimer("physics_total");
        
        // Apply physics to each cell
        timers.startTimer("apply_physics");
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                CellB& cell = at(x, y);
                if (!cell.isEmpty() && !cell.isWall()) {
                    worldRulesBNew_->applyPhysics(cell, x, y, deltaTimeSeconds, *this);
                }
            }
        }
        timers.stopTimer("apply_physics");

        // Update pressure systems
        timers.startTimer("update_pressures");
        worldRulesBNew_->updatePressures(*this, deltaTimeSeconds);
        timers.stopTimer("update_pressures");

        // Apply pressure forces
        timers.startTimer("apply_pressure_forces");
        worldRulesBNew_->applyPressureForces(*this, deltaTimeSeconds);
        timers.stopTimer("apply_pressure_forces");

        // Process transfers
        timers.startTimer("process_transfers");
        processTransfers(deltaTimeSeconds);
        timers.stopTimer("process_transfers");

        // Apply moves
        timers.startTimer("apply_moves");
        applyMoves();
        timers.stopTimer("apply_moves");

        timestep++;
        timers.stopTimer("physics_total");
    } else {
        spdlog::warn("No world rules set for WorldB");
    }
}

void WorldB::processTransfers(double deltaTimeSeconds)
{
    moves.clear();

    if (!worldRulesBNew_) return;

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            CellB& cell = at(x, y);
            
            if (cell.isEmpty() || cell.isWall()) continue;
            
            if (worldRulesBNew_->shouldTransfer(cell, x, y, *this)) {
                bool shouldTransferX, shouldTransferY;
                int targetX, targetY;
                Vector2d comOffset;
                
                worldRulesBNew_->calculateTransferDirection(
                    cell, shouldTransferX, shouldTransferY, 
                    targetX, targetY, comOffset, x, y, *this);
                
                double totalMass = cell.fill_ratio;
                if (totalMass > 0.0) {
                    bool success = worldRulesBNew_->attemptTransfer(
                        cell, x, y, targetX, targetY, comOffset, totalMass, *this);
                    
                    if (!success) {
                        worldRulesBNew_->handleTransferFailure(
                            cell, x, y, targetX, targetY, 
                            shouldTransferX, shouldTransferY, *this);
                    }
                }
            }
        }
    }
}

void WorldB::applyMoves()
{
    // Shuffle moves to randomize order for fairness
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(moves.begin(), moves.end(), g);

    for (const auto& move : moves) {
        CellB& fromCell = at(move.fromX, move.fromY);
        CellB& toCell = at(move.toX, move.toY);

        // Validate move
        if (fromCell.material != move.materialType) {
            spdlog::warn("Material type mismatch in move: from {} to {}", 
                        static_cast<int>(fromCell.material), 
                        static_cast<int>(move.materialType));
            continue;
        }

        if (move.amount <= 0.0) continue;

        // Remove material from source
        double actualRemoved = fromCell.removeMaterial(move.amount);
        
        if (actualRemoved > 0.0) {
            // Add material to target
            double actualAdded = toCell.addMaterial(move.materialType, actualRemoved);
            
            if (actualAdded < actualRemoved) {
                // Couldn't add all material - put remainder back
                double remainder = actualRemoved - actualAdded;
                fromCell.addMaterial(move.materialType, remainder);
                spdlog::trace("Move overflow: {} remainder put back", remainder);
            } else {
                // Transfer momentum using center of mass
                if (!toCell.isEmpty()) {
                    double fromMass = actualAdded;
                    double toMass = toCell.fill_ratio - actualAdded;
                    
                    if (toMass > 0.0) {
                        // Weighted average of COM
                        Vector2d newCom = (fromCell.com * fromMass + toCell.com * toMass) / (fromMass + toMass);
                        toCell.com = newCom + move.comOffset;
                        
                        // Also transfer some velocity
                        Vector2d newVel = (fromCell.v * fromMass + toCell.v * toMass) / (fromMass + toMass);
                        toCell.v = newVel;
                    } else {
                        toCell.com = fromCell.com + move.comOffset;
                        toCell.v = fromCell.v;
                    }
                }
            }
        }
    }

    moves.clear();
}

void WorldB::initializeTestMaterials()
{
    spdlog::info("Initializing WorldB test materials");
    
    // Add some dirt in the middle-left area
    for (uint32_t y = height/4; y < 3*height/4; ++y) {
        for (uint32_t x = width/4; x < width/2; ++x) {
            if (!at(x, y).isWall()) {
                at(x, y).setMaterial(MaterialType::DIRT, 0.8);
            }
        }
    }
    
    // Add some water in the middle-right area  
    for (uint32_t y = height/3; y < 2*height/3; ++y) {
        for (uint32_t x = width/2 + 2; x < 3*width/4; ++x) {
            if (!at(x, y).isWall()) {
                at(x, y).setMaterial(MaterialType::WATER, 0.6);
            }
        }
    }
    
    // Add a few wood blocks
    for (uint32_t y = height/6; y < height/3; ++y) {
        for (uint32_t x = width/6; x < width/4; ++x) {
            if (!at(x, y).isWall()) {
                at(x, y).setMaterial(MaterialType::WOOD, 1.0);
            }
        }
    }
    
    spdlog::debug("WorldB test materials initialized");
}

void WorldB::validateState(const std::string& context) const
{
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const CellB& cell = at(x, y);
            cell.validateState(context + " at (" + std::to_string(x) + "," + std::to_string(y) + ")");
        }
    }
}
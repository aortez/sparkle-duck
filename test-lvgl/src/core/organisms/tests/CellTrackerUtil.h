#pragma once

#include "core/Cell.h"
#include "core/GridOfCells.h"
#include "core/MaterialType.h"
#include "core/Vector2i.h"
#include "core/World.h"
#include "core/WorldData.h"
#include "core/organisms/Tree.h"
#include <iostream>
#include <unordered_map>
#include <vector>

namespace DirtSim {

/**
 * Utility for tracking cell physics state over time and detecting displacements.
 *
 * Useful for debugging physics behavior and validating that cells stay in expected positions.
 */
class CellTracker {
public:
    struct CellFrameData {
        Vector2d com;
        Vector2d velocity;
        Vector2d pending_force;
        Vector2d bone_force;
        Vector2d gravity_force;
        Vector2d support_force;
        Vector2d cohesion_force;
        Vector2d adhesion_force;
        Vector2d viscous_force;
        Vector2d friction_force;
        Vector2d pressure_force;
        bool has_support;
    };

    struct TrackedCell {
        MaterialType material;
        int frame_added;
    };

    struct PrevCellState {
        MaterialType material;
        uint32_t org_id;
    };

    CellTracker(World& world, TreeId organism_id, size_t history_size = 20)
        : world_(world), organism_id_(organism_id), history_size_(history_size)
    {}

    // Start tracking a cell at a given position.
    void trackCell(const Vector2i& pos, MaterialType material, int frame);

    // Record current state of all tracked cells (call after advanceTime).
    void recordFrame(int frame);

    // Check for displaced cells and report. Returns true if any cell was displaced.
    bool checkForDisplacements(int frame);

    // Detect newly added cells by comparing old and new cell sets.
    void detectNewCells(
        const std::unordered_map<Vector2i, PrevCellState>& cells_before,
        const std::unordered_set<Vector2i>& cells_after,
        int frame);

    // Overload for set-based comparison.
    void detectNewCells(
        const std::unordered_set<Vector2i>& cells_before,
        const std::unordered_set<Vector2i>& cells_after,
        int frame);

    // Print history for a specific cell.
    void printHistory(const Vector2i& pos, int current_frame) const;

    // Print table header for detailed frame-by-frame output.
    void printTableHeader() const;

    // Print a table row showing current state of all tracked cells.
    void printTableRow(int frame, bool force_print = false) const;

private:
    World& world_;
    TreeId organism_id_;
    size_t history_size_;

    // Tracked cells and their history.
    std::unordered_map<Vector2i, TrackedCell> tracked_cells_;
    std::unordered_map<Vector2i, std::vector<CellFrameData>> cell_history_;
    std::unordered_map<Vector2i, PrevCellState> prev_frame_cells_;
};

} // namespace DirtSim

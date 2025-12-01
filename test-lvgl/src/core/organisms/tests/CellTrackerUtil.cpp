#include "CellTrackerUtil.h"
#include "core/WorldDiagramGeneratorEmoji.h"
#include <iomanip>

namespace DirtSim {

void CellTracker::trackCell(const Vector2i& pos, MaterialType material, int frame)
{
    tracked_cells_[pos] = TrackedCell{ material, frame };
}

void CellTracker::recordFrame(int /*frame*/)
{
    for (const auto& [pos, tracked] : tracked_cells_) {
        if (static_cast<uint32_t>(pos.x) < world_.getData().width
            && static_cast<uint32_t>(pos.y) < world_.getData().height) {
            const Cell& cell = world_.getData().at(pos.x, pos.y);
            const auto& debug = world_.getGrid().debugAt(pos.x, pos.y);

            cell_history_[pos].push_back(
                CellFrameData{ .com = cell.com,
                               .velocity = cell.velocity,
                               .pending_force = cell.pending_force,
                               .bone_force = debug.accumulated_bone_force,
                               .gravity_force = debug.accumulated_gravity_force,
                               .support_force = debug.accumulated_support_force,
                               .cohesion_force = debug.accumulated_com_cohesion_force,
                               .adhesion_force = debug.accumulated_adhesion_force,
                               .viscous_force = debug.accumulated_viscous_force,
                               .friction_force = debug.accumulated_friction_force,
                               .pressure_force = debug.accumulated_pressure_force,
                               .has_support = cell.has_any_support });

            // Keep only last N frames.
            if (cell_history_[pos].size() > history_size_) {
                cell_history_[pos].erase(cell_history_[pos].begin());
            }
        }
    }

    // Update previous frame states.
    prev_frame_cells_.clear();
    for (const auto& [pos, tracked] : tracked_cells_) {
        const Cell& cell = world_.getData().at(pos.x, pos.y);
        prev_frame_cells_[pos] = PrevCellState{ cell.material_type, cell.organism_id };
    }
}

bool CellTracker::checkForDisplacements(int frame)
{
    bool any_displaced = false;
    std::vector<Vector2i> to_remove;

    for (const auto& [pos, tracked] : tracked_cells_) {
        const Cell& cell = world_.getData().at(pos.x, pos.y);

        bool cell_moved = (cell.organism_id != organism_id_)
            || (cell.material_type != tracked.material) || (cell.fill_ratio < 0.5);

        if (cell_moved) {
            any_displaced = true;
            std::cout << "\n⚠️  CELL MOVED at frame " << frame << " (added at frame "
                      << tracked.frame_added << ")\n";
            std::cout << "  Expected: " << getMaterialName(tracked.material) << " at (" << pos.x
                      << ", " << pos.y << ")\n";

            printHistory(pos, frame);

            // Print current cell stats.
            const auto& debug = world_.getGrid().debugAt(pos.x, pos.y);
            std::cout << "  Current cell stats:\n";
            std::cout << "    Material: " << getMaterialName(cell.material_type) << "\n";
            std::cout << "    Fill: " << std::setprecision(2) << std::fixed << cell.fill_ratio
                      << "\n";
            std::cout << "    COM: (" << cell.com.x << ", " << cell.com.y << ")\n";
            std::cout << "    Velocity: (" << cell.velocity.x << ", " << cell.velocity.y << ")\n";
            std::cout << "    Pending Force: (" << cell.pending_force.x << ", "
                      << cell.pending_force.y << ")\n";
            std::cout << "    Gravity: (" << debug.accumulated_gravity_force.x << ", "
                      << debug.accumulated_gravity_force.y << ")\n";
            std::cout << "    Support: (" << debug.accumulated_support_force.x << ", "
                      << debug.accumulated_support_force.y << ")\n";
            std::cout << "    Cohesion: (" << debug.accumulated_com_cohesion_force.x << ", "
                      << debug.accumulated_com_cohesion_force.y << ")\n";
            std::cout << "    Adhesion: (" << debug.accumulated_adhesion_force.x << ", "
                      << debug.accumulated_adhesion_force.y << ")\n";
            std::cout << "    Viscosity: (" << debug.accumulated_viscous_force.x << ", "
                      << debug.accumulated_viscous_force.y << ")\n";
            std::cout << "    Has Support: " << (cell.has_any_support ? "yes" : "no") << "\n";

            std::cout << "  Diagram:\n"
                      << WorldDiagramGeneratorEmoji::generateEmojiDiagram(world_) << "\n";

            to_remove.push_back(pos);
        }
    }

    for (const auto& pos : to_remove) {
        tracked_cells_.erase(pos);
    }

    return any_displaced;
}

void CellTracker::detectNewCells(
    const std::unordered_map<Vector2i, PrevCellState>& cells_before,
    const std::unordered_set<Vector2i>& cells_after,
    int frame)
{
    for (const auto& pos : cells_after) {
        auto it = cells_before.find(pos);
        bool is_new = (it == cells_before.end());

        if (is_new) {
            const Cell& cell = world_.getData().at(pos.x, pos.y);
            trackCell(pos, cell.material_type, frame);
            std::cout << "\n🌱 NEW CELL at frame " << frame << ": "
                      << getMaterialName(cell.material_type) << " at (" << pos.x << ", " << pos.y
                      << ")\n";
            std::cout << WorldDiagramGeneratorEmoji::generateEmojiDiagram(world_) << "\n";
        }
    }
}

void CellTracker::detectNewCells(
    const std::unordered_set<Vector2i>& cells_before,
    const std::unordered_set<Vector2i>& cells_after,
    int frame)
{
    for (const auto& pos : cells_after) {
        if (cells_before.find(pos) == cells_before.end()) {
            const Cell& cell = world_.getData().at(pos.x, pos.y);
            trackCell(pos, cell.material_type, frame);
            std::cout << "\n🌱 NEW CELL at frame " << frame << ": "
                      << getMaterialName(cell.material_type) << " at (" << pos.x << ", " << pos.y
                      << ")\n";
            std::cout << WorldDiagramGeneratorEmoji::generateEmojiDiagram(world_) << "\n";
        }
    }
}

void CellTracker::printHistory(const Vector2i& pos, int current_frame) const
{
    if (cell_history_.count(pos) > 0) {
        const auto& history = cell_history_.at(pos);
        std::cout << "  History (last " << history.size() << " frames before move):\n";

        // Show HORIZONTAL (X) forces - this is where oscillation happens!
        std::cout << "    HORIZONTAL FORCES (X direction):\n";
        std::cout
            << "    Frame | COM.x | Vel.x | Coh.x | Visc.x | Fric.x | Sum   | Total.x | Diff\n";
        std::cout
            << "    ------|-------|-------|-------|--------|--------|-------|---------|-----\n";
        for (size_t h = 0; h < history.size(); h++) {
            const auto& fd = history[h];
            int frame_num = current_frame - static_cast<int>(history.size() - h);
            // Calculate sum of known forces.
            double known_sum = fd.cohesion_force.x + fd.adhesion_force.x + fd.viscous_force.x
                + fd.friction_force.x + fd.gravity_force.x + fd.support_force.x
                + fd.pressure_force.x;
            double diff = fd.pending_force.x - known_sum;
            std::cout << "    " << std::setw(5) << frame_num << " | " << std::setw(5) << std::fixed
                      << std::setprecision(2) << fd.com.x << " | " << std::setw(5) << fd.velocity.x
                      << " | " << std::setw(5) << fd.cohesion_force.x << " | " << std::setw(6)
                      << fd.viscous_force.x << " | " << std::setw(6) << fd.friction_force.x << " | "
                      << std::setw(5) << known_sum << " | " << std::setw(7) << fd.pending_force.x
                      << " | " << std::setw(4) << diff << "\n";
        }

        std::cout << "\n    VERTICAL FORCES (Y direction):\n";
        std::cout
            << "    Frame | COM.y | Vel.y | Grav.y | Supp.y | Coh.y | Visc.y | Total.y | Sup\n";
        std::cout
            << "    ------|-------|-------|--------|--------|-------|--------|---------|----\n";
        for (size_t h = 0; h < history.size(); h++) {
            const auto& fd = history[h];
            int frame_num = current_frame - static_cast<int>(history.size() - h);
            std::cout << "    " << std::setw(5) << frame_num << " | " << std::setw(5) << std::fixed
                      << std::setprecision(2) << fd.com.y << " | " << std::setw(5) << fd.velocity.y
                      << " | " << std::setw(6) << fd.gravity_force.y << " | " << std::setw(6)
                      << fd.support_force.y << " | " << std::setw(5) << fd.cohesion_force.y << " | "
                      << std::setw(6) << fd.viscous_force.y << " | " << std::setw(7)
                      << fd.pending_force.y << " | " << (fd.has_support ? "Y" : "N") << "\n";
        }
    }
}

void CellTracker::printTableHeader() const
{
    std::cout << "Frame | Cell    | COM        | Velocity   | Grav | Supp | Coh  | Adh  | "
                 "Total\n";
    std::cout << "------|---------|------------|------------|------|------|------|------|------\n";
}

void CellTracker::printTableRow(int frame, bool force_print) const
{
    if (!force_print && frame >= 20 && frame % 10 != 0) {
        return;
    }

    for (const auto& [pos, tracked] : tracked_cells_) {
        const Cell& cell = world_.getData().at(pos.x, pos.y);
        const auto& debug = world_.getGrid().debugAt(pos.x, pos.y);

        std::cout << std::setw(5) << frame << " | " << getMaterialName(tracked.material)[0] << "("
                  << pos.x << "," << pos.y << ") | (" << std::setw(5) << std::fixed
                  << std::setprecision(2) << cell.com.x << "," << std::setw(5) << cell.com.y
                  << ") | (" << std::setw(5) << cell.velocity.x << "," << std::setw(5)
                  << cell.velocity.y << ") | " << std::setw(4) << debug.accumulated_gravity_force.y
                  << " | " << std::setw(4) << debug.accumulated_support_force.y << " | "
                  << std::setw(4) << debug.accumulated_com_cohesion_force.y << " | " << std::setw(4)
                  << debug.accumulated_adhesion_force.y << " | " << std::setw(5)
                  << cell.pending_force.y << "\n";
    }
}

} // namespace DirtSim

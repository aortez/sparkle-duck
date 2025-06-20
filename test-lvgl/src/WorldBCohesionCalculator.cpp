#include "WorldBCohesionCalculator.h"
#include "CellB.h"
#include "MaterialType.h"
#include "WorldB.h"
#include "WorldBSupportCalculator.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <cmath>

WorldBCohesionCalculator::WorldBCohesionCalculator(const WorldB& world)
    : world_(world), support_calculator_(std::make_unique<WorldBSupportCalculator>(world))
{}

const CellB& WorldBCohesionCalculator::getCellAt(uint32_t x, uint32_t y) const
{
    // Direct access to CellB through WorldB
    return world_.at(x, y);
}

bool WorldBCohesionCalculator::isValidCell(int x, int y) const
{
    return x >= 0 && y >= 0 && static_cast<uint32_t>(x) < world_.getWidth()
        && static_cast<uint32_t>(y) < world_.getHeight();
}

WorldBCohesionCalculator::CohesionForce WorldBCohesionCalculator::calculateCohesionForce(
    uint32_t x, uint32_t y) const
{
    const CellB& cell = getCellAt(x, y);
    if (cell.isEmpty()) {
        return { 0.0, 0 };
    }

    const MaterialProperties& props = getMaterialProperties(cell.getMaterialType());
    double material_cohesion = props.cohesion;
    uint32_t connected_neighbors = 0;

    // Check all 8 neighbors (including diagonals)
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0) continue; // Skip self

            int nx = static_cast<int>(x) + dx;
            int ny = static_cast<int>(y) + dy;

            if (isValidCell(nx, ny)) {
                const CellB& neighbor = getCellAt(nx, ny);
                if (neighbor.getMaterialType() == cell.getMaterialType()
                    && neighbor.getFillRatio() > WorldBCohesionCalculator::MIN_MATTER_THRESHOLD) {

                    // Weight by neighbor's fill ratio for partial cells
                    connected_neighbors += 1; // Count as 1 neighbor
                }
            }
        }
    }

    // Check for metal neighbors that provide structural support
    uint32_t metal_neighbors = 0;
    if (cell.getMaterialType() == MaterialType::METAL) {
        // Count metal neighbors for structural support
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                if (dx == 0 && dy == 0) continue; // Skip self
                
                int nx = static_cast<int>(x) + dx;
                int ny = static_cast<int>(y) + dy;
                
                if (isValidCell(nx, ny)) {
                    const CellB& neighbor = getCellAt(nx, ny);
                    if (neighbor.getMaterialType() == MaterialType::METAL 
                        && neighbor.getFillRatio() > 0.5) {
                        metal_neighbors++;
                    }
                }
            }
        }
    }

    // Use directional support for realistic physics
    bool has_vertical = support_calculator_->hasVerticalSupport(x, y);
    bool has_horizontal = support_calculator_->hasHorizontalSupport(x, y);

    // Calculate support factor based on directional support
    double support_factor;
    
    // Metal with sufficient metal neighbors provides full structural support
    if (metal_neighbors >= 2) {
        // Metal network provides full support in all directions
        support_factor = 1.0;
        spdlog::trace(
            "Metal structural network support for {} at ({},{}) with {} metal neighbors",
            getMaterialName(cell.getMaterialType()),
            x,
            y,
            metal_neighbors);
    }
    else if (has_vertical) {
        // Full cohesion with vertical support (load-bearing from below)
        support_factor = 1.0;
        spdlog::trace(
            "Full vertical support for {} at ({},{})",
            getMaterialName(cell.getMaterialType()),
            x,
            y);
    }
    else if (has_horizontal) {
        // Reduced cohesion with only horizontal support (rigid lateral connections)
        support_factor = 0.5;
        spdlog::trace(
            "Horizontal support only for {} at ({},{})",
            getMaterialName(cell.getMaterialType()),
            x,
            y);
    }
    else {
        // Minimal cohesion without structural support
        support_factor = WorldB::MIN_SUPPORT_FACTOR; // 0.05
        spdlog::trace(
            "No structural support for {} at ({},{})",
            getMaterialName(cell.getMaterialType()),
            x,
            y);
    }

    // Resistance magnitude = cohesion × connection strength × own fill ratio × support factor
    double resistance =
        material_cohesion * connected_neighbors * cell.getFillRatio() * support_factor;

    spdlog::trace(
        "Cohesion calculation for {} at ({},{}): neighbors={}, vertical_support={}, "
        "horizontal_support={}, support_factor={:.2f}, resistance={:.3f}",
        getMaterialName(cell.getMaterialType()),
        x,
        y,
        connected_neighbors,
        has_vertical,
        has_horizontal,
        support_factor,
        resistance);

    return { resistance, connected_neighbors };
}

WorldBCohesionCalculator::COMCohesionForce WorldBCohesionCalculator::calculateCOMCohesionForce(
    uint32_t x, uint32_t y, uint32_t com_cohesion_range) const
{
    const CellB& cell = getCellAt(x, y);
    if (cell.isEmpty()) {
        return { { 0.0, 0.0 }, 0.0, { 0.0, 0.0 }, 0 };
    }

    // Calculate cell world position including COM offset
    Vector2d cell_world_pos(
        static_cast<double>(x) + cell.getCOM().x, static_cast<double>(y) + cell.getCOM().y);
    Vector2d neighbor_center_sum(0.0, 0.0);
    double total_weight = 0.0;
    uint32_t connection_count = 0;

    // Check all neighbors within COM cohesion range for same-material connections
    int range = static_cast<int>(com_cohesion_range);
    for (int dx = -range; dx <= range; dx++) {
        for (int dy = -range; dy <= range; dy++) {
            if (dx == 0 && dy == 0) continue;

            int nx = static_cast<int>(x) + dx;
            int ny = static_cast<int>(y) + dy;

            if (isValidCell(nx, ny)) {
                const CellB& neighbor = getCellAt(nx, ny);
                if (neighbor.getMaterialType() == cell.getMaterialType()
                    && neighbor.getFillRatio() > WorldBCohesionCalculator::MIN_MATTER_THRESHOLD) {

                    // Get neighbor's world position including its COM offset
                    Vector2d neighbor_world_pos(
                        static_cast<double>(nx) + neighbor.getCOM().x,
                        static_cast<double>(ny) + neighbor.getCOM().y);
                    double weight = neighbor.getFillRatio();

                    neighbor_center_sum += neighbor_world_pos * weight;
                    total_weight += weight;
                    connection_count++;
                }
            }
        }
    }

    if (connection_count == 0 || total_weight < WorldBCohesionCalculator::MIN_MATTER_THRESHOLD) {
        return { { 0.0, 0.0 }, 0.0, { 0.0, 0.0 }, 0 };
    }

    // Calculate weighted center of connected neighbors
    Vector2d neighbor_center = neighbor_center_sum / total_weight;

    // Force direction: toward the center of neighbors
    Vector2d force_direction = neighbor_center - cell_world_pos;
    double distance = force_direction.magnitude();

    if (distance < 0.001) { // Avoid division by zero
        return { { 0.0, 0.0 }, 0.0, neighbor_center, connection_count };
    }

    force_direction.normalize();

    // Force magnitude: cohesion strength × connection count × distance factor × fill ratio
    const MaterialProperties& props = getMaterialProperties(cell.getMaterialType());
    double base_cohesion = props.cohesion;
    double distance_factor = std::min(distance, 2.0); // Cap at 2 cell distances
    // Calculate max possible connections for this range: (2*range+1)² - 1 (excluding center)
    double max_connections = static_cast<double>((2 * range + 1) * (2 * range + 1) - 1);
    double connection_factor =
        static_cast<double>(connection_count) / max_connections; // Normalize to [0,1]
    double force_magnitude =
        base_cohesion * connection_factor * distance_factor * cell.getFillRatio();

    // Prevent excessive COM forces
    double max_com_force = base_cohesion * 2.0; // Cap at 2x base cohesion
    force_magnitude = std::min(force_magnitude, max_com_force);

    Vector2d final_force = force_direction * force_magnitude;

    spdlog::trace(
        "COM cohesion for {} at ({},{}): connections={}, distance={:.3f}, force_mag={:.3f}, "
        "direction=({:.3f},{:.3f})",
        getMaterialName(cell.getMaterialType()),
        x,
        y,
        connection_count,
        distance,
        force_magnitude,
        final_force.x,
        final_force.y);

    return { final_force, force_magnitude, neighbor_center, connection_count };
}

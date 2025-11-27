#include "WorldAdhesionCalculator.h"
#include "Cell.h"
#include "PhysicsSettings.h"
#include "World.h"
#include "WorldData.h"
#include <cmath>

using namespace DirtSim;

WorldAdhesionCalculator::AdhesionForce WorldAdhesionCalculator::calculateAdhesionForce(
    const World& world, uint32_t x, uint32_t y) const
{
    const Cell& cell = getCellAt(world, x, y);
    if (cell.isEmpty()) {
        return { { 0.0, 0.0 }, 0.0, MaterialType::AIR, 0 };
    }

    const MaterialProperties& props = getMaterialProperties(cell.material_type);
    Vector2d total_force(0.0, 0.0);
    uint32_t contact_count = 0;
    MaterialType strongest_attractor = MaterialType::AIR;
    double max_adhesion = 0.0;

    // Check all 8 neighbors for different materials.
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0) continue;

            int nx = static_cast<int>(x) + dx;
            int ny = static_cast<int>(y) + dy;

            if (isValidCell(world, nx, ny)) {
                const Cell& neighbor = getCellAt(world, nx, ny);

                // Skip same material and AIR neighbors (AIR has adhesion=0.0).
                if (neighbor.material_type == cell.material_type
                    || neighbor.material_type == MaterialType::AIR) {
                    continue;
                }

                if (neighbor.fill_ratio > MIN_MATTER_THRESHOLD) {

                    // Calculate mutual adhesion (geometric mean)
                    const MaterialProperties& neighbor_props =
                        getMaterialProperties(neighbor.material_type);
                    double mutual_adhesion = std::sqrt(props.adhesion * neighbor_props.adhesion);

                    // Direction vector toward neighbor (normalized)
                    Vector2d direction(static_cast<double>(dx), static_cast<double>(dy));
                    direction.normalize();

                    // Force strength weighted by fill ratios and distance.
                    double distance_weight =
                        (std::abs(dx) + std::abs(dy) == 1) ? 1.0 : 0.707; // Adjacent vs diagonal.
                    double force_strength =
                        mutual_adhesion * neighbor.fill_ratio * cell.fill_ratio * distance_weight;

                    total_force += direction * force_strength;
                    contact_count++;

                    if (mutual_adhesion > max_adhesion) {
                        max_adhesion = mutual_adhesion;
                        strongest_attractor = neighbor.material_type;
                    }
                }
            }
        }
    }

    return { total_force, total_force.mag(), strongest_attractor, contact_count };
}

WorldAdhesionCalculator::AdhesionForce WorldAdhesionCalculator::calculateAdhesionForce(
    const World& world, uint32_t x, uint32_t y, const MaterialNeighborhood& mat_n) const
{
    const Cell& cell = getCellAt(world, x, y);
    if (cell.isEmpty()) {
        return { { 0.0, 0.0 }, 0.0, MaterialType::AIR, 0 };
    }

    const MaterialProperties& props = getMaterialProperties(cell.material_type);
    const MaterialType my_material = mat_n.getCenterMaterial();
    Vector2d total_force(0.0, 0.0);
    uint32_t contact_count = 0;
    MaterialType strongest_attractor = MaterialType::AIR;
    double max_adhesion = 0.0;

    // Check all 8 neighbors for different materials.
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0) continue;

            int nx = static_cast<int>(x) + dx;
            int ny = static_cast<int>(y) + dy;

            // Multi-stage cache filtering (bounds check handled by cache).
            // Stage 1: Material difference check (pure cache - no cell access).
            MaterialType neighbor_material = mat_n.getMaterial(dx, dy);
            if (neighbor_material == my_material || neighbor_material == MaterialType::AIR) {
                continue;
            }

            // At this point: different material type, guaranteed non-empty due to AIR conversion.
            // Fetch cell only when we know we need it.
            const Cell& neighbor = getCellAt(world, nx, ny);

            // Calculate mutual adhesion (geometric mean).
            const MaterialProperties& neighbor_props = getMaterialProperties(neighbor_material);
            double mutual_adhesion = std::sqrt(props.adhesion * neighbor_props.adhesion);

            // Direction vector toward neighbor (normalized).
            Vector2d direction(static_cast<double>(dx), static_cast<double>(dy));
            direction.normalize();

            // Force strength weighted by fill ratios and distance.
            double distance_weight =
                (std::abs(dx) + std::abs(dy) == 1) ? 1.0 : 0.707; // Adjacent vs diagonal.
            double force_strength =
                mutual_adhesion * neighbor.fill_ratio * cell.fill_ratio * distance_weight;

            total_force += direction * force_strength;
            contact_count++;

            if (mutual_adhesion > max_adhesion) {
                max_adhesion = mutual_adhesion;
                strongest_attractor = neighbor_material;
            }
        }
    }

    return { total_force, total_force.mag(), strongest_attractor, contact_count };
}
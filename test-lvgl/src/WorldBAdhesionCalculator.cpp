#include "WorldBAdhesionCalculator.h"
#include "CellB.h"
#include "WorldB.h"
#include <cmath>

WorldBAdhesionCalculator::WorldBAdhesionCalculator(const WorldB& world)
    : WorldBCalculatorBase(world)
{}

WorldBAdhesionCalculator::AdhesionForce WorldBAdhesionCalculator::calculateAdhesionForce(
    uint32_t x, uint32_t y) const
{
    const CellB& cell = getCellAt(x, y);
    if (cell.isEmpty()) {
        return { { 0.0, 0.0 }, 0.0, MaterialType::AIR, 0 };
    }

    const MaterialProperties& props = getMaterialProperties(cell.getMaterialType());
    Vector2d total_force(0.0, 0.0);
    uint32_t contact_count = 0;
    MaterialType strongest_attractor = MaterialType::AIR;
    double max_adhesion = 0.0;

    // Check all 8 neighbors for different materials
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0) continue;

            int nx = static_cast<int>(x) + dx;
            int ny = static_cast<int>(y) + dy;

            if (isValidCell(nx, ny)) {
                const CellB& neighbor = getCellAt(nx, ny);

                if (neighbor.getMaterialType() != cell.getMaterialType()
                    && neighbor.getFillRatio() > MIN_MATTER_THRESHOLD) {

                    // Calculate mutual adhesion (geometric mean)
                    const MaterialProperties& neighbor_props =
                        getMaterialProperties(neighbor.getMaterialType());
                    double mutual_adhesion = std::sqrt(props.adhesion * neighbor_props.adhesion);

                    // Direction vector toward neighbor (normalized)
                    Vector2d direction(static_cast<double>(dx), static_cast<double>(dy));
                    direction.normalize();

                    // Force strength weighted by fill ratios and distance
                    double distance_weight =
                        (std::abs(dx) + std::abs(dy) == 1) ? 1.0 : 0.707; // Adjacent vs diagonal
                    double force_strength = mutual_adhesion * neighbor.getFillRatio()
                        * cell.getFillRatio() * distance_weight;

                    total_force += direction * force_strength;
                    contact_count++;

                    if (mutual_adhesion > max_adhesion) {
                        max_adhesion = mutual_adhesion;
                        strongest_attractor = neighbor.getMaterialType();
                    }
                }
            }
        }
    }

    return { total_force, total_force.mag(), strongest_attractor, contact_count };
}
#include "WorldFrictionCalculator.h"
#include "Cell.h"
#include "GridOfCells.h"
#include "PhysicsSettings.h"
#include "World.h"
#include "WorldData.h"
#include "spdlog/spdlog.h"
#include <cmath>

using namespace DirtSim;

WorldFrictionCalculator::WorldFrictionCalculator(GridOfCells& grid) : grid_(grid)
{}

void WorldFrictionCalculator::calculateAndApplyFrictionForces(World& world, double /* deltaTime */)
{
    if (friction_strength_ <= 0.0) {
        return;
    }

    // Clear friction forces from previous frame.
    for (uint32_t y = 0; y < grid_.getHeight(); ++y) {
        for (uint32_t x = 0; x < grid_.getWidth(); ++x) {
            grid_.debugAt(x, y).accumulated_friction_force = Vector2d{};
        }
    }

    // STEP 1: Calculate friction forces and accumulate in debug info.
    if (GridOfCells::USE_CACHE) {
        accumulateFrictionForces(world);
    }
    else {
        std::vector<ContactInterface> contacts = detectContactInterfaces(world);
        accumulateFrictionFromContacts(world, contacts);
    }

    // Apply accumulated friction forces to cells with constraint.
    // Tunable: Allow limited momentum transfer while preventing oscillations.
    static constexpr double FRICTION_MOMENTUM_TRANSFER_LIMIT = 1;

    WorldData& data = world.getData();
    for (uint32_t y = 0; y < data.height; ++y) {
        for (uint32_t x = 0; x < data.width; ++x) {
            Cell& cell = data.at(x, y);
            if (cell.isEmpty() || cell.isWall()) {
                continue;
            }

            Vector2d friction_force = grid_.debugAt(x, y).accumulated_friction_force;

            // CONSTRAINT: Friction should primarily oppose motion.
            // Allow limited momentum transfer when friction aids motion.
            double dot_product = friction_force.dot(cell.velocity);
            if (dot_product > 0.0) {
                // Friction aids motion - limit to prevent oscillations.
                double friction_mag = friction_force.magnitude();
                double velocity_mag = cell.velocity.magnitude();

                if (velocity_mag > 0.001) {
                    // Limit aiding friction to small fraction of velocity.
                    double max_aiding = velocity_mag * FRICTION_MOMENTUM_TRANSFER_LIMIT;

                    if (friction_mag > max_aiding) {
                        friction_force = friction_force.normalize() * max_aiding;
                    }
                }
                else {
                    // Near-zero velocity - don't allow friction to create motion.
                    friction_force = Vector2d(0.0, 0.0);
                }
            }

            cell.addPendingForce(friction_force);
        }
    }
}

void WorldFrictionCalculator::accumulateFrictionForces(World& world)
{
    // Cache data reference to avoid Pimpl indirection in inner loop.
    WorldData& data = world.getData();
    const uint32_t width = data.width;
    const uint32_t height = data.height;

    // Iterate over all cells.
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            Cell& cellA = data.at(x, y);

            // Skip empty cells, walls, and fluids.
            // Fluids don't have Coulomb friction - they have viscosity instead.
            if (cellA.isEmpty() || cellA.isWall() || isMaterialFluid(cellA.material_type)) {
                continue;
            }

            // Check only cardinal (non-diagonal) neighbors for friction.
            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    if (dx == 0 && dy == 0) continue;

                    // Skip diagonal neighbors - only cardinal contacts.
                    bool is_diagonal = (dx != 0 && dy != 0);
                    if (is_diagonal) continue;

                    int nx = static_cast<int>(x) + dx;
                    int ny = static_cast<int>(y) + dy;

                    // Only process each pair once (avoid double-counting).
                    if (nx < static_cast<int>(x)) continue;
                    if (nx == static_cast<int>(x) && ny <= static_cast<int>(y)) continue;

                    if (!isValidCell(world, nx, ny)) continue;

                    const Cell& cellB = data.at(nx, ny);

                    // Skip if neighbor is empty, wall, or fluid.
                    if (cellB.isEmpty() || cellB.isWall() || isMaterialFluid(cellB.material_type)) {
                        continue;
                    }

                    // Calculate interface normal (from A to B).
                    Vector2d interface_normal =
                        Vector2d{ static_cast<double>(dx), static_cast<double>(dy) };
                    interface_normal = interface_normal.normalize();

                    // Calculate normal force.
                    const double normal_force = calculateNormalForce(
                        world, cellA, cellB, Vector2i(x, y), Vector2i(nx, ny), interface_normal);

                    // Skip if normal force is too small.
                    if (normal_force < MIN_NORMAL_FORCE) {
                        continue;
                    }

                    // Calculate relative velocity.
                    const Vector2d relative_velocity = cellA.velocity - cellB.velocity;

                    // Calculate tangential velocity.
                    const Vector2d tangential_velocity =
                        calculateTangentialVelocity(relative_velocity, interface_normal);

                    const double tangential_speed = tangential_velocity.magnitude();

                    // Skip if tangential velocity is negligible.
                    if (tangential_speed < MIN_TANGENTIAL_SPEED) {
                        continue;
                    }

                    // Calculate friction coefficient.
                    const MaterialProperties& propsA = getMaterialProperties(cellA.material_type);
                    const MaterialProperties& propsB = getMaterialProperties(cellB.material_type);
                    double friction_coefficient =
                        calculateFrictionCoefficient(tangential_speed, propsA, propsB);

                    // Calculate and apply friction force immediately.
                    double accumulated_friction_force_magnitude =
                        friction_coefficient * normal_force * friction_strength_;

                    Vector2d friction_direction = tangential_velocity.normalize() * -1.0;
                    Vector2d accumulated_friction_force =
                        friction_direction * accumulated_friction_force_magnitude;

                    // STEP 1: Calculate and accumulate friction forces (don't apply yet).
                    // Store in debug info for later application.
                    grid_.debugAt(x, y).accumulated_friction_force += accumulated_friction_force;
                    grid_.debugAt(nx, ny).accumulated_friction_force +=
                        (-accumulated_friction_force);

                    spdlog::trace(
                        "Friction force: ({},{}) <-> ({},{}): normal_force={:.4f}, mu={:.3f}, "
                        "tangential_speed={:.4f}, force=({:.4f},{:.4f})",
                        x,
                        y,
                        nx,
                        ny,
                        normal_force,
                        friction_coefficient,
                        tangential_speed,
                        accumulated_friction_force.x,
                        accumulated_friction_force.y);
                }
            }
        }
    }
    // Friction forces will be applied in calculateAndApplyFrictionForces STEP 2.
}

std::vector<WorldFrictionCalculator::ContactInterface> WorldFrictionCalculator::
    detectContactInterfaces(const World& world) const
{
    std::vector<ContactInterface> contacts;

    const uint32_t width = world.getData().width;
    const uint32_t height = world.getData().height;

    // Iterate over all cells.
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const Cell& cellA = getCellAt(world, x, y);

            // Skip empty cells, walls, and fluids.
            // Fluids don't have Coulomb friction - they have viscosity instead.
            if (cellA.isEmpty() || cellA.isWall() || isMaterialFluid(cellA.material_type)) {
                continue;
            }

            const MaterialProperties& propsA = getMaterialProperties(cellA.material_type);

            // Check only cardinal (non-diagonal) neighbors for friction.
            // Diagonal contacts don't make physical sense in a grid system.
            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    if (dx == 0 && dy == 0) continue;

                    // Skip diagonal neighbors - only cardinal contacts.
                    const bool is_diagonal = (dx != 0 && dy != 0);
                    if (is_diagonal) continue;

                    const int nx = static_cast<int>(x) + dx;
                    const int ny = static_cast<int>(y) + dy;

                    // Only process each pair once (avoid double-counting).
                    // Process only if neighbor is to the right or below.
                    if (nx < static_cast<int>(x)) continue;
                    if (nx == static_cast<int>(x) && ny <= static_cast<int>(y)) continue;

                    if (!isValidCell(world, nx, ny)) continue;

                    const Cell& cellB = getCellAt(world, nx, ny);

                    // Skip if neighbor is empty, wall, or fluid.
                    if (cellB.isEmpty() || cellB.isWall() || isMaterialFluid(cellB.material_type)) {
                        continue;
                    }

                    // Create contact interface.
                    ContactInterface contact;
                    contact.cell_A_pos = Vector2i(x, y);
                    contact.cell_B_pos = Vector2i(nx, ny);

                    // Calculate interface normal (from A to B).
                    contact.interface_normal =
                        Vector2d{ static_cast<double>(dx), static_cast<double>(dy) };
                    contact.interface_normal = contact.interface_normal.normalize();

                    // Calculate contact area (cardinal = 1.0, diagonal = 0.707).
                    contact.contact_area = (std::abs(dx) + std::abs(dy) == 1) ? 1.0 : 0.707;

                    // Calculate normal force.
                    contact.normal_force = calculateNormalForce(
                        world,
                        cellA,
                        cellB,
                        contact.cell_A_pos,
                        contact.cell_B_pos,
                        contact.interface_normal);

                    // Skip if normal force is too small.
                    if (contact.normal_force < MIN_NORMAL_FORCE) {
                        continue;
                    }

                    // Calculate relative velocity.
                    contact.relative_velocity = cellA.velocity - cellB.velocity;

                    // Calculate tangential velocity.
                    contact.tangential_velocity = calculateTangentialVelocity(
                        contact.relative_velocity, contact.interface_normal);

                    double tangential_speed = contact.tangential_velocity.magnitude();

                    // Skip if tangential velocity is negligible.
                    if (tangential_speed < MIN_TANGENTIAL_SPEED) {
                        continue;
                    }

                    // Calculate friction coefficient.
                    const MaterialProperties& propsB = getMaterialProperties(cellB.material_type);
                    contact.friction_coefficient =
                        calculateFrictionCoefficient(tangential_speed, propsA, propsB);

                    contacts.push_back(contact);
                }
            }
        }
    }

    spdlog::trace("Detected {} friction contact interfaces", contacts.size());
    return contacts;
}

double WorldFrictionCalculator::calculateNormalForce(
    const World& world,
    const Cell& cellA,
    const Cell& cellB,
    const Vector2i& /* posA */,
    const Vector2i& /* posB */,
    const Vector2d& interface_normal) const
{
    double normal_force = 0.0;

    // Source 1: Pressure difference across interface.
    // Higher pressure in A pushes against B.
    double pressureA = cellA.pressure;
    double pressureB = cellB.pressure;
    double pressure_difference = pressureA - pressureB;

    if (pressure_difference > 0.0) {
        // Scale pressure to force (pressure is already in force-like units in our system).
        normal_force += pressure_difference * cellA.fill_ratio;
    }

    // Source 2: Weight for vertical contacts.
    // If B is below A (interface normal points downward), weight of A creates normal force.
    double gravity_magnitude = world.getPhysicsSettings().gravity;

    if (interface_normal.y > 0.5) { // B is below A (normal points down).
        double massA = cellA.getMass();
        double weight = massA * gravity_magnitude;
        normal_force += weight;
    }
    else if (interface_normal.y < -0.5) { // A is below B (normal points up).
        double massB = cellB.getMass();
        double weight = massB * gravity_magnitude;
        normal_force += weight;
    }

    return normal_force;
}

double WorldFrictionCalculator::calculateFrictionCoefficient(
    double tangential_speed,
    const MaterialProperties& propsA,
    const MaterialProperties& propsB) const
{
    // Use geometric mean for inter-material friction coefficients.
    double static_friction =
        std::sqrt(propsA.static_friction_coefficient * propsB.static_friction_coefficient);
    double kinetic_friction =
        std::sqrt(propsA.kinetic_friction_coefficient * propsB.kinetic_friction_coefficient);

    // Use average for velocity thresholds.
    double stick_velocity = (propsA.stick_velocity + propsB.stick_velocity) / 2.0;
    double transition_width =
        (propsA.friction_transition_width + propsB.friction_transition_width) / 2.0;

    // Below stick velocity, use full static friction.
    if (tangential_speed < stick_velocity) {
        return static_friction;
    }

    // Calculate smooth transition parameter.
    double t = (tangential_speed - stick_velocity) / transition_width;

    // Clamp t to [0, 1] range.
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;

    // Smooth cubic interpolation (3t² - 2t³).
    double smooth_t = t * t * (3.0 - 2.0 * t);

    // Interpolate between static and kinetic friction.
    return static_friction * (1.0 - smooth_t) + kinetic_friction * smooth_t;
}

Vector2d WorldFrictionCalculator::calculateTangentialVelocity(
    const Vector2d& relative_velocity, const Vector2d& interface_normal) const
{
    // Decompose relative velocity into normal and tangential components.
    // Tangential = relative_velocity - (relative_velocity · normal) * normal.

    double normal_component = relative_velocity.dot(interface_normal);
    Vector2d normal_velocity = interface_normal * normal_component;
    Vector2d tangential_velocity = relative_velocity - normal_velocity;

    return tangential_velocity;
}

void WorldFrictionCalculator::accumulateFrictionFromContacts(
    World& world, const std::vector<ContactInterface>& contacts)
{
    (void)world;
    for (const ContactInterface& contact : contacts) {
        // Calculate friction force magnitude.
        double accumulated_friction_force_magnitude =
            contact.friction_coefficient * contact.normal_force * friction_strength_;

        // Direction: opposite to tangential relative velocity.
        Vector2d friction_direction = contact.tangential_velocity.normalize() * -1.0;

        Vector2d accumulated_friction_force =
            friction_direction * accumulated_friction_force_magnitude;

        // STEP 1: Accumulate friction forces (don't apply yet).
        grid_.debugAt(contact.cell_A_pos.x, contact.cell_A_pos.y).accumulated_friction_force +=
            accumulated_friction_force;
        grid_.debugAt(contact.cell_B_pos.x, contact.cell_B_pos.y).accumulated_friction_force +=
            (-accumulated_friction_force);

        spdlog::trace(
            "Friction force: ({},{}) <-> ({},{}): normal_force={:.4f}, mu={:.3f}, "
            "tangential_speed={:.4f}, force=({:.4f},{:.4f})",
            contact.cell_A_pos.x,
            contact.cell_A_pos.y,
            contact.cell_B_pos.x,
            contact.cell_B_pos.y,
            contact.normal_force,
            contact.friction_coefficient,
            contact.tangential_velocity.magnitude(),
            accumulated_friction_force.x,
            accumulated_friction_force.y);
    }
}

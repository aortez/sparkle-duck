#include "Cell.h"
#include "World.h" // For MIN_MATTER_THRESHOLD constant.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>
#include <string>

#include "lvgl/lvgl.h"
#include "spdlog/spdlog.h"

using namespace DirtSim;

// Cell is now an aggregate - no constructors needed!

void Cell::setFillRatio(double ratio)
{
    fill_ratio = std::clamp(ratio, 0.0, 1.0);

    // If fill ratio becomes effectively zero, convert to air.
    if (fill_ratio < MIN_FILL_THRESHOLD) {
        material_type = MaterialType::AIR;
        fill_ratio = 0.0;
        velocity = Vector2d{0.0, 0.0};
        com = Vector2d{0.0, 0.0};

        // Clear all pressure values when cell becomes empty.
        pressure = 0.0;
        hydrostatic_component = 0.0;
        dynamic_component = 0.0;
        pressure_gradient = Vector2d{0.0, 0.0};
    }
}

void Cell::setCOM(const Vector2d& newCom)
{
    com = Vector2d{std::clamp(newCom.x, COM_MIN, COM_MAX), std::clamp(newCom.y, COM_MIN, COM_MAX)};
}

double Cell::getMass() const
{
    if (isEmpty()) {
        return 0.0;
    }
    return fill_ratio * getMaterialDensity(material_type);
}

double Cell::getEffectiveDensity() const
{
    return fill_ratio * getMaterialDensity(material_type);
}

double Cell::addMaterial(MaterialType type, double amount)
{
    if (amount <= 0.0) {
        return 0.0;
    }

    // If we're empty, accept any material type.
    if (isEmpty()) {
        material_type = type;
        const double added = std::min(amount, 1.0);
        fill_ratio = added;
        return added;
    }

    // If different material type, no mixing allowed.
    if (material_type != type) {
        return 0.0;
    }

    // Add to existing material.
    const double capacity = getCapacity();
    const double added = std::min(amount, capacity);
    fill_ratio += added;

    if (added > 0.0) {
    }

    return added;
}

double Cell::addMaterialWithPhysics(
    MaterialType type,
    double amount,
    const Vector2d& source_com,
    const Vector2d& newVel,
    const Vector2d& boundary_normal)
{
    if (amount <= 0.0) {
        return 0.0;
    }

    // If we're empty, accept any material type with trajectory-based COM.
    if (isEmpty()) {
        material_type = type;
        const double added = std::min(amount, 1.0);
        fill_ratio = added;

        // Calculate realistic landing position based on boundary crossing.
        com = calculateTrajectoryLanding(source_com, newVel, boundary_normal);
        velocity = newVel; // Preserve velocity through transfer.

        return added;
    }

    // If different material type, no mixing allowed.
    if (material_type != type) {
        return 0.0;
    }

    // Add to existing material with momentum conservation.
    const double capacity = getCapacity();
    const double added = std::min(amount, capacity);

    if (added > 0.0) {
        // Enhanced momentum conservation: new_COM = (m1*COM1 + m2*COM2)/(m1+m2).
        const double existing_mass = getMass();
        const double added_mass = added * getMaterialProperties(material_type).density;
        const double total_mass = existing_mass + added_mass;

        // Calculate incoming material's COM in target cell space.
        Vector2d incoming_com = calculateTrajectoryLanding(source_com, newVel, boundary_normal);

        if (total_mass > World::MIN_MATTER_THRESHOLD) {
            // Weighted average of COM positions.
            com = (com * existing_mass + incoming_com * added_mass) / total_mass;

            // Momentum conservation for velocity.
            velocity = (velocity * existing_mass + newVel * added_mass) / total_mass;
        }

        fill_ratio += added;
    }

    return added;
}

double Cell::removeMaterial(double amount)
{
    if (isEmpty() || amount <= 0.0) {
        return 0.0;
    }

    const double removed = std::min(amount, fill_ratio);
    fill_ratio -= removed;

    // Check if we became empty.
    if (fill_ratio < MIN_FILL_THRESHOLD) {
        clear();
    }

    return removed;
}

double Cell::transferTo(Cell& target, double amount)
{
    if (isEmpty() || amount <= 0.0) {
        return 0.0;
    }

    // Calculate how much we can actually transfer.
    const double available = std::min(amount, fill_ratio);
    const double accepted = target.addMaterial(material_type, available);

    // Remove the accepted amount from this cell.
    if (accepted > 0.0) {
        removeMaterial(accepted);
    }

    return accepted;
}

double Cell::transferToWithPhysics(Cell& target, double amount, const Vector2d& boundary_normal)
{
    if (isEmpty() || amount <= 0.0) {
        return 0.0;
    }

    // Calculate how much we can actually transfer.
    const double available = std::min(amount, fill_ratio);

    // Use physics-aware method with current COM and velocity.
    const double accepted =
        target.addMaterialWithPhysics(material_type, available, com, velocity, boundary_normal);

    // Remove the accepted amount from this cell.
    if (accepted > 0.0) {
        removeMaterial(accepted);
    }

    return accepted;
}

void Cell::replaceMaterial(MaterialType type, double fill_ratio)
{
    material_type = type;
    setFillRatio(fill_ratio);

    // Reset physics state when replacing material.
    velocity = Vector2d{0.0, 0.0};
    com = Vector2d{0.0, 0.0};
}

void Cell::clear()
{
    material_type = MaterialType::AIR;
    fill_ratio = 0.0;
    velocity = Vector2d{0.0, 0.0};
    com = Vector2d{0.0, 0.0};

    // Clear all pressure values when cell becomes empty.
    pressure = 0.0;
    hydrostatic_component = 0.0;
    dynamic_component = 0.0;
    pressure_gradient = Vector2d{0.0, 0.0};
}

void Cell::limitVelocity(
    double max_velocityper_timestep,
    double damping_threshold_per_timestep,
    double damping_factor_per_timestep,
    double /* deltaTime */)
{
    const double speed = velocity.mag();

    // Apply velocity limits directly (parameters are already per-timestep).
    // The parameters define absolute velocity limits per physics timestep.

    // Apply maximum velocity limit.
    if (speed > max_velocityper_timestep) {
        velocity = velocity * (max_velocityper_timestep / speed);
    }

    // Apply damping when above threshold.
    if (speed > damping_threshold_per_timestep) {
        // Apply damping factor directly (parameters already account for timestep).
        velocity = velocity * (1.0 - damping_factor_per_timestep);
    }
}

void Cell::clampCOM()
{
    com.x = std::clamp(com.x, COM_MIN, COM_MAX);
    com.y = std::clamp(com.y, COM_MIN, COM_MAX);
}

bool Cell::shouldTransfer() const
{
    if (isEmpty() || isWall()) {
        return false;
    }

    // Transfer only when COM reaches cell boundaries (±1.0) per GridMechanics.md.
    return std::abs(com.x) >= 1.0 || std::abs(com.y) >= 1.0;
}

Vector2d Cell::getTransferDirection() const
{
    // Determine primary transfer direction based on COM position at boundaries.
    Vector2d direction(0.0, 0.0);

    if (com.x >= 1.0) {
        direction.x = 1.0; // Transfer right when COM reaches right boundary.
    }
    else if (com.x <= -1.0) {
        direction.x = -1.0; // Transfer left when COM reaches left boundary.
    }

    if (com.y >= 1.0) {
        direction.y = 1.0; // Transfer down when COM reaches bottom boundary.
    }
    else if (com.y <= -1.0) {
        direction.y = -1.0; // Transfer up when COM reaches top boundary.
    }

    return direction;
}

Vector2d Cell::calculateTrajectoryLanding(
    const Vector2d& source_com, const Vector2d& velocity, const Vector2d& boundary_normal) const
{
    // Calculate where material actually crosses the boundary.
    Vector2d boundary_crossing_point = source_com;

    // Determine which boundary was crossed and calculate intersection.
    if (std::abs(boundary_normal.x) > 0.5) {
        // Crossing left/right boundary.
        double boundary_x = (boundary_normal.x > 0) ? 1.0 : -1.0;
        double crossing_ratio = (boundary_x - source_com.x) / velocity.x;
        if (std::abs(velocity.x) > 1e-6) {
            boundary_crossing_point.x = boundary_x;
            boundary_crossing_point.y = source_com.y + velocity.y * crossing_ratio;
        }
    }
    else if (std::abs(boundary_normal.y) > 0.5) {
        // Crossing top/bottom boundary.
        double boundary_y = (boundary_normal.y > 0) ? 1.0 : -1.0;
        double crossing_ratio = (boundary_y - source_com.y) / velocity.y;
        if (std::abs(velocity.y) > 1e-6) {
            boundary_crossing_point.y = boundary_y;
            boundary_crossing_point.x = source_com.x + velocity.x * crossing_ratio;
        }
    }

    // Transform crossing point to target cell coordinate space.
    Vector2d target_com = boundary_crossing_point;

    // Wrap coordinates across boundary.
    if (std::abs(boundary_normal.x) > 0.5) {
        // Material crossed left/right - wrap X coordinate.
        target_com.x = (boundary_normal.x > 0) ? -1.0 : 1.0;
    }
    if (std::abs(boundary_normal.y) > 0.5) {
        // Material crossed top/bottom - wrap Y coordinate.
        target_com.y = (boundary_normal.y > 0) ? -1.0 : 1.0;
    }

    // Clamp to valid COM bounds.
    target_com.x = std::clamp(target_com.x, COM_MIN, COM_MAX);
    target_com.y = std::clamp(target_com.y, COM_MIN, COM_MAX);

    return target_com;
}

std::string Cell::toString() const
{
    std::ostringstream oss;
    oss << getMaterialName(material_type) << "(fill=" << fill_ratio << ", com=[" << com.x << ","
        << com.y << "]" << ", vel=[" << velocity.x << "," << velocity.y << "]" << ")";
    return oss.str();
}

// =================================================================.
// CELLINTERFACE IMPLEMENTATION.
// =================================================================.

void Cell::addDirt(double amount)
{
    if (amount <= 0.0) return;
    addMaterial(MaterialType::DIRT, amount);
}

void Cell::addWater(double amount)
{
    if (amount <= 0.0) return;
    addMaterial(MaterialType::WATER, amount);
}

void Cell::addDirtWithVelocity(double amount, const Vector2d& newVel)
{
    if (amount <= 0.0) return;

    // Store current fill ratio to calculate momentum.
    double oldFill = fill_ratio;
    double actualAdded = addMaterial(MaterialType::DIRT, amount);

    if (actualAdded > 0.0) {
        // Update velocity based on momentum conservation.
        double newFill = fill_ratio;
        if (newFill > 0.0) {
            // Weighted average of existing velocity and new velocity.
            velocity = (velocity * oldFill + newVel * actualAdded) / newFill;
        }
        else {
            velocity = newVel;
        }
    }
}

void Cell::addDirtWithCOM(double amount, const Vector2d& newCom, const Vector2d& newVel)
{
    if (amount <= 0.0) return;

    // Store current state to calculate weighted averages.
    double oldFill = fill_ratio;
    Vector2d oldCOM = com;
    Vector2d oldVelocity = velocity;

    double actualAdded = addMaterial(MaterialType::DIRT, amount);

    if (actualAdded > 0.0) {
        double newFill = fill_ratio;
        if (newFill > 0.0) {
            // Weighted average of existing COM and new COM.
            com = (oldCOM * oldFill + newCom * actualAdded) / newFill;
            clampCOM(); // Ensure COM stays in bounds.

            // Weighted average of existing velocity and new velocity.
            velocity = (oldVelocity * oldFill + newVel * actualAdded) / newFill;
        }
        else {
            com = newCom;
            velocity = newVel;
        }
    }
}

double Cell::getTotalMaterial() const
{
    return fill_ratio;
}

// =================================================================.
// RENDERING METHODS.
// =================================================================.

std::string Cell::toAsciiCharacter() const
{
    if (isEmpty()) {
        return "  "; // Two spaces for empty cells (2x1 format).
    }

    // Choose character based on material type.
    char material_char;
    switch (material_type) {
        case MaterialType::AIR:
            return "  "; // Two spaces for air.
        case MaterialType::DIRT:
            material_char = '#';
            break;
        case MaterialType::WATER:
            material_char = '~';
            break;
        case MaterialType::WOOD:
            material_char = 'W';
            break;
        case MaterialType::SAND:
            material_char = '.';
            break;
        case MaterialType::METAL:
            material_char = 'M';
            break;
        case MaterialType::LEAF:
            material_char = 'L';
            break;
        case MaterialType::WALL:
            material_char = '|';
            break;
        default:
            material_char = '?';
            break;
    }

    // Convert fill ratio to 0-9 scale.
    int fill_level = static_cast<int>(std::round(fill_ratio * 9.0));
    fill_level = std::clamp(fill_level, 0, 9);

    // Return 2-character representation: material + fill level.
    return std::string(1, material_char) + std::to_string(fill_level);
}

// =================================================================
// JSON SERIALIZATION
// =================================================================


#include "ReflectSerializer.h"

nlohmann::json Cell::toJson() const
{
    return ReflectSerializer::to_json(*this);
}

Cell Cell::fromJson(const nlohmann::json& json)
{
    return ReflectSerializer::from_json<Cell>(json);
}

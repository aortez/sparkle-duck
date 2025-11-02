#include "Cell.h"
#include "Cell.h"   // For WIDTH/HEIGHT constants.
#include "World.h" // For MIN_MATTER_THRESHOLD constant.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>
#include <string>

#include "lvgl/lvgl.h"
#include "spdlog/spdlog.h"

Cell::Cell()
    : material_type_(MaterialType::AIR),
      fill_ratio_(0.0),
      com_(0.0, 0.0),
      velocity_(0.0, 0.0),
      pressure_(0.0),
      hydrostatic_component_(0.0),
      dynamic_component_(0.0),
      pressure_gradient_(0.0, 0.0),
      accumulated_cohesion_force_(0.0, 0.0),
      accumulated_adhesion_force_(0.0, 0.0),
      accumulated_com_cohesion_force_(0.0, 0.0),
      pending_force_(0.0, 0.0),
      cached_friction_coefficient_(1.0)
{}

Cell::Cell(MaterialType type, double fill)
    : material_type_(type),
      fill_ratio_(std::clamp(fill, 0.0, 1.0)),
      com_(0.0, 0.0),
      velocity_(0.0, 0.0),
      pressure_(0.0),
      hydrostatic_component_(0.0),
      dynamic_component_(0.0),
      pressure_gradient_(0.0, 0.0),
      accumulated_cohesion_force_(0.0, 0.0),
      accumulated_adhesion_force_(0.0, 0.0),
      accumulated_com_cohesion_force_(0.0, 0.0),
      pending_force_(0.0, 0.0),
      cached_friction_coefficient_(1.0)
{}

Cell::~Cell() = default;

Cell::Cell(const Cell& other)
    : material_type_(other.material_type_),
      fill_ratio_(other.fill_ratio_),
      com_(other.com_),
      velocity_(other.velocity_),
      pressure_(other.pressure_),
      hydrostatic_component_(other.hydrostatic_component_),
      dynamic_component_(other.dynamic_component_),
      pressure_gradient_(other.pressure_gradient_),
      accumulated_cohesion_force_(other.accumulated_cohesion_force_),
      accumulated_adhesion_force_(other.accumulated_adhesion_force_),
      accumulated_com_cohesion_force_(other.accumulated_com_cohesion_force_),
      pending_force_(other.pending_force_),
      cached_friction_coefficient_(other.cached_friction_coefficient_)
{}

// Assignment operator - don't copy LVGL objects, they'll be recreated on demand.
Cell& Cell::operator=(const Cell& other)
{
    if (this != &other) {
        material_type_ = other.material_type_;
        fill_ratio_ = other.fill_ratio_;
        com_ = other.com_;
        velocity_ = other.velocity_;
        pressure_ = other.pressure_;
        hydrostatic_component_ = other.hydrostatic_component_;
        dynamic_component_ = other.dynamic_component_;
        pressure_gradient_ = other.pressure_gradient_;
        accumulated_cohesion_force_ = other.accumulated_cohesion_force_;
        accumulated_adhesion_force_ = other.accumulated_adhesion_force_;
        accumulated_com_cohesion_force_ = other.accumulated_com_cohesion_force_;
        pending_force_ = other.pending_force_;
        cached_friction_coefficient_ = other.cached_friction_coefficient_;
    }
    return *this;
}

void Cell::setFillRatio(double ratio)
{
    fill_ratio_ = std::clamp(ratio, 0.0, 1.0);

    // If fill ratio becomes effectively zero, convert to air.
    if (fill_ratio_ < MIN_FILL_THRESHOLD) {
        material_type_ = MaterialType::AIR;
        fill_ratio_ = 0.0;
        velocity_ = Vector2d{0.0, 0.0};
        com_ = Vector2d{0.0, 0.0};

        // Clear all pressure values when cell becomes empty.
        pressure_ = 0.0;
        hydrostatic_component_ = 0.0;
        dynamic_component_ = 0.0;
        pressure_gradient_ = Vector2d{0.0, 0.0};
    }
}

void Cell::setCOM(const Vector2d& com)
{
    com_ = Vector2d{std::clamp(com.x, COM_MIN, COM_MAX), std::clamp(com.y, COM_MIN, COM_MAX)};
}

double Cell::getMass() const
{
    if (isEmpty()) {
        return 0.0;
    }
    return fill_ratio_ * getMaterialDensity(material_type_);
}

double Cell::getEffectiveDensity() const
{
    return fill_ratio_ * getMaterialDensity(material_type_);
}

double Cell::addMaterial(MaterialType type, double amount)
{
    if (amount <= 0.0) {
        return 0.0;
    }

    // If we're empty, accept any material type.
    if (isEmpty()) {
        material_type_ = type;
        const double added = std::min(amount, 1.0);
        fill_ratio_ = added;
        return added;
    }

    // If different material type, no mixing allowed.
    if (material_type_ != type) {
        return 0.0;
    }

    // Add to existing material.
    const double capacity = getCapacity();
    const double added = std::min(amount, capacity);
    fill_ratio_ += added;

    if (added > 0.0) {
    }

    return added;
}

double Cell::addMaterialWithPhysics(
    MaterialType type,
    double amount,
    const Vector2d& source_com,
    const Vector2d& velocity,
    const Vector2d& boundary_normal)
{
    if (amount <= 0.0) {
        return 0.0;
    }

    // If we're empty, accept any material type with trajectory-based COM.
    if (isEmpty()) {
        material_type_ = type;
        const double added = std::min(amount, 1.0);
        fill_ratio_ = added;

        // Calculate realistic landing position based on boundary crossing.
        com_ = calculateTrajectoryLanding(source_com, velocity, boundary_normal);
        velocity_ = velocity; // Preserve velocity through transfer.

        return added;
    }

    // If different material type, no mixing allowed.
    if (material_type_ != type) {
        return 0.0;
    }

    // Add to existing material with momentum conservation.
    const double capacity = getCapacity();
    const double added = std::min(amount, capacity);

    if (added > 0.0) {
        // Enhanced momentum conservation: new_COM = (m1*COM1 + m2*COM2)/(m1+m2).
        const double existing_mass = getMass();
        const double added_mass = added * getMaterialProperties(material_type_).density;
        const double total_mass = existing_mass + added_mass;

        // Calculate incoming material's COM in target cell space.
        Vector2d incoming_com = calculateTrajectoryLanding(source_com, velocity, boundary_normal);

        if (total_mass > World::MIN_MATTER_THRESHOLD) {
            // Weighted average of COM positions.
            com_ = (com_ * existing_mass + incoming_com * added_mass) / total_mass;

            // Momentum conservation for velocity.
            velocity_ = (velocity_ * existing_mass + velocity * added_mass) / total_mass;
        }

        fill_ratio_ += added;
    }

    return added;
}

double Cell::removeMaterial(double amount)
{
    if (isEmpty() || amount <= 0.0) {
        return 0.0;
    }

    const double removed = std::min(amount, fill_ratio_);
    fill_ratio_ -= removed;

    // Check if we became empty.
    if (fill_ratio_ < MIN_FILL_THRESHOLD) {
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
    const double available = std::min(amount, fill_ratio_);
    const double accepted = target.addMaterial(material_type_, available);

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
    const double available = std::min(amount, fill_ratio_);

    // Use physics-aware method with current COM and velocity.
    const double accepted =
        target.addMaterialWithPhysics(material_type_, available, com_, velocity_, boundary_normal);

    // Remove the accepted amount from this cell.
    if (accepted > 0.0) {
        removeMaterial(accepted);
    }

    return accepted;
}

void Cell::replaceMaterial(MaterialType type, double fill_ratio)
{
    material_type_ = type;
    setFillRatio(fill_ratio);

    // Reset physics state when replacing material.
    velocity_ = Vector2d{0.0, 0.0};
    com_ = Vector2d{0.0, 0.0};
}

void Cell::clear()
{
    material_type_ = MaterialType::AIR;
    fill_ratio_ = 0.0;
    velocity_ = Vector2d{0.0, 0.0};
    com_ = Vector2d{0.0, 0.0};

    // Clear all pressure values when cell becomes empty.
    pressure_ = 0.0;
    hydrostatic_component_ = 0.0;
    dynamic_component_ = 0.0;
    pressure_gradient_ = Vector2d{0.0, 0.0};
}

void Cell::limitVelocity(
    double max_velocity_per_timestep,
    double damping_threshold_per_timestep,
    double damping_factor_per_timestep,
    double /* deltaTime */)
{
    const double speed = velocity_.mag();

    // Apply velocity limits directly (parameters are already per-timestep).
    // The parameters define absolute velocity limits per physics timestep.

    // Apply maximum velocity limit.
    if (speed > max_velocity_per_timestep) {
        velocity_ = velocity_ * (max_velocity_per_timestep / speed);
    }

    // Apply damping when above threshold.
    if (speed > damping_threshold_per_timestep) {
        // Apply damping factor directly (parameters already account for timestep).
        velocity_ = velocity_ * (1.0 - damping_factor_per_timestep);
    }
}

void Cell::clampCOM()
{
    com_.x = std::clamp(com_.x, COM_MIN, COM_MAX);
    com_.y = std::clamp(com_.y, COM_MIN, COM_MAX);
}

bool Cell::shouldTransfer() const
{
    if (isEmpty() || isWall()) {
        return false;
    }

    // Transfer only when COM reaches cell boundaries (±1.0) per GridMechanics.md.
    return std::abs(com_.x) >= 1.0 || std::abs(com_.y) >= 1.0;
}

Vector2d Cell::getTransferDirection() const
{
    // Determine primary transfer direction based on COM position at boundaries.
    Vector2d direction(0.0, 0.0);

    if (com_.x >= 1.0) {
        direction.x = 1.0; // Transfer right when COM reaches right boundary.
    }
    else if (com_.x <= -1.0) {
        direction.x = -1.0; // Transfer left when COM reaches left boundary.
    }

    if (com_.y >= 1.0) {
        direction.y = 1.0; // Transfer down when COM reaches bottom boundary.
    }
    else if (com_.y <= -1.0) {
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
    oss << getMaterialName(material_type_) << "(fill=" << fill_ratio_ << ", com=[" << com_.x << ","
        << com_.y << "]" << ", vel=[" << velocity_.x << "," << velocity_.y << "]" << ")";
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

void Cell::addDirtWithVelocity(double amount, const Vector2d& velocity)
{
    if (amount <= 0.0) return;

    // Store current fill ratio to calculate momentum.
    double oldFill = fill_ratio_;
    double actualAdded = addMaterial(MaterialType::DIRT, amount);

    if (actualAdded > 0.0) {
        // Update velocity based on momentum conservation.
        double newFill = fill_ratio_;
        if (newFill > 0.0) {
            // Weighted average of existing velocity and new velocity.
            velocity_ = (velocity_ * oldFill + velocity * actualAdded) / newFill;
        }
        else {
            velocity_ = velocity;
        }
    }
}

void Cell::addWaterWithVelocity(double amount, const Vector2d& velocity)
{
    if (amount <= 0.0) return;

    // Store current fill ratio to calculate momentum.
    double oldFill = fill_ratio_;
    double actualAdded = addMaterial(MaterialType::WATER, amount);

    if (actualAdded > 0.0) {
        // Update velocity based on momentum conservation.
        double newFill = fill_ratio_;
        if (newFill > 0.0) {
            // Weighted average of existing velocity and new velocity.
            velocity_ = (velocity_ * oldFill + velocity * actualAdded) / newFill;
        }
        else {
            velocity_ = velocity;
        }
    }
}

void Cell::addDirtWithCOM(double amount, const Vector2d& com, const Vector2d& velocity)
{
    if (amount <= 0.0) return;

    // Store current state to calculate weighted averages.
    double oldFill = fill_ratio_;
    Vector2d oldCOM = com_;
    Vector2d oldVelocity = velocity_;

    double actualAdded = addMaterial(MaterialType::DIRT, amount);

    if (actualAdded > 0.0) {
        double newFill = fill_ratio_;
        if (newFill > 0.0) {
            // Weighted average of existing COM and new COM.
            com_ = (oldCOM * oldFill + com * actualAdded) / newFill;
            clampCOM(); // Ensure COM stays in bounds.

            // Weighted average of existing velocity and new velocity.
            velocity_ = (oldVelocity * oldFill + velocity * actualAdded) / newFill;
        }
        else {
            com_ = com;
            velocity_ = velocity;
        }
    }
}

double Cell::getTotalMaterial() const
{
    return fill_ratio_;
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
    switch (material_type_) {
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
    int fill_level = static_cast<int>(std::round(fill_ratio_ * 9.0));
    fill_level = std::clamp(fill_level, 0, 9);

    // Return 2-character representation: material + fill level.
    return std::string(1, material_char) + std::to_string(fill_level);
}

// =================================================================
// JSON SERIALIZATION
// =================================================================

rapidjson::Value Cell::toJson(rapidjson::Document::AllocatorType& allocator) const
{
    rapidjson::Value json(rapidjson::kObjectType);

    // Material state.
    json.AddMember("material_type", materialTypeToJson(material_type_, allocator), allocator);
    json.AddMember("fill_ratio", fill_ratio_, allocator);

    // Physics state.
    json.AddMember("com", com_.toJson(allocator), allocator);
    json.AddMember("velocity", velocity_.toJson(allocator), allocator);
    json.AddMember("pressure", pressure_, allocator);

    // Optional: Include pressure components for debugging/visualization.
    json.AddMember("hydrostatic_component", hydrostatic_component_, allocator);
    json.AddMember("dynamic_component", dynamic_component_, allocator);

    return json;
}

Cell Cell::fromJson(const rapidjson::Value& json)
{
    if (!json.IsObject()) {
        throw std::runtime_error("Cell::fromJson: JSON value must be an object");
    }

    // Validate required fields.
    if (!json.HasMember("material_type") || !json.HasMember("fill_ratio")) {
        throw std::runtime_error("Cell::fromJson: Missing required fields 'material_type' or 'fill_ratio'");
    }

    // Parse material type and fill ratio.
    MaterialType material_type = materialTypeFromJson(json["material_type"]);
    double fill_ratio = json["fill_ratio"].GetDouble();

    // Create cell with material and fill.
    Cell cell(material_type, fill_ratio);

    // Parse optional physics state.
    if (json.HasMember("com")) {
        cell.com_ = Vector2d::fromJson(json["com"]);
    }

    if (json.HasMember("velocity")) {
        cell.velocity_ = Vector2d::fromJson(json["velocity"]);
    }

    if (json.HasMember("pressure")) {
        cell.pressure_ = json["pressure"].GetDouble();
    }

    // Parse optional pressure components.
    if (json.HasMember("hydrostatic_component")) {
        cell.hydrostatic_component_ = json["hydrostatic_component"].GetDouble();
    }

    if (json.HasMember("dynamic_component")) {
        cell.dynamic_component_ = json["dynamic_component"].GetDouble();
    }

    return cell;
}

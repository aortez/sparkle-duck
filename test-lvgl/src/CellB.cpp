#include "CellB.h"
#include "Cell.h"   // For WIDTH/HEIGHT constants.
#include "WorldB.h" // For MIN_MATTER_THRESHOLD constant.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>
#include <string>

#include "lvgl/lvgl.h"
#include "spdlog/spdlog.h"

CellB::CellB()
    : material_type_(MaterialType::AIR),
      fill_ratio_(0.0),
      com_(0.0, 0.0),
      velocity_(0.0, 0.0),
      hydrostatic_pressure_(0.0),
      dynamic_pressure_(0.0),
      pressure_gradient_(0.0, 0.0),
      accumulated_cohesion_force_(0.0, 0.0),
      accumulated_adhesion_force_(0.0, 0.0),
      accumulated_com_cohesion_force_(0.0, 0.0),
      pending_force_(0.0, 0.0),
      canvas_(nullptr),
      needs_redraw_(true)
{}

CellB::CellB(MaterialType type, double fill)
    : material_type_(type),
      fill_ratio_(std::clamp(fill, 0.0, 1.0)),
      com_(0.0, 0.0),
      velocity_(0.0, 0.0),
      hydrostatic_pressure_(0.0),
      dynamic_pressure_(0.0),
      pressure_gradient_(0.0, 0.0),
      accumulated_cohesion_force_(0.0, 0.0),
      accumulated_adhesion_force_(0.0, 0.0),
      accumulated_com_cohesion_force_(0.0, 0.0),
      pending_force_(0.0, 0.0),
      canvas_(nullptr),
      needs_redraw_(true)
{}

CellB::~CellB()
{
    // Clean up the LVGL canvas object if it exists.
    if (canvas_ != nullptr) {
        lv_obj_del(canvas_);
        canvas_ = nullptr;
    }
}

// Copy constructor - don't copy LVGL objects, they'll be recreated on demand.
CellB::CellB(const CellB& other)
    : material_type_(other.material_type_),
      fill_ratio_(other.fill_ratio_),
      com_(other.com_),
      velocity_(other.velocity_),
      hydrostatic_pressure_(other.hydrostatic_pressure_),
      dynamic_pressure_(other.dynamic_pressure_),
      pressure_gradient_(other.pressure_gradient_),
      accumulated_cohesion_force_(other.accumulated_cohesion_force_),
      accumulated_adhesion_force_(other.accumulated_adhesion_force_),
      accumulated_com_cohesion_force_(other.accumulated_com_cohesion_force_),
      pending_force_(other.pending_force_),
      buffer_(other.buffer_.size()), // Create new buffer with same size.
      canvas_(nullptr),              // Don't copy LVGL object.
      needs_redraw_(true)            // New copy needs redraw.
{
    // Buffer contents will be regenerated when drawn.
}

// Assignment operator - don't copy LVGL objects, they'll be recreated on demand.
CellB& CellB::operator=(const CellB& other)
{
    if (this != &other) {
        // Clean up existing canvas before assignment.
        if (canvas_ != nullptr) {
            lv_obj_del(canvas_);
            canvas_ = nullptr;
        }

        // Copy the physics state.
        material_type_ = other.material_type_;
        fill_ratio_ = other.fill_ratio_;
        com_ = other.com_;
        velocity_ = other.velocity_;
        hydrostatic_pressure_ = other.hydrostatic_pressure_;
        dynamic_pressure_ = other.dynamic_pressure_;
        pressure_gradient_ = other.pressure_gradient_;
        accumulated_cohesion_force_ = other.accumulated_cohesion_force_;
        accumulated_adhesion_force_ = other.accumulated_adhesion_force_;
        accumulated_com_cohesion_force_ = other.accumulated_com_cohesion_force_;
        pending_force_ = other.pending_force_;

        // Resize buffer if needed but don't copy contents.
        buffer_.resize(other.buffer_.size());

        // Don't copy LVGL object - keep canvas_ as nullptr.
        // canvas_ stays as nullptr.
        needs_redraw_ = true; // Assignment means we need redraw.
    }
    return *this;
}

void CellB::setFillRatio(double ratio)
{
    fill_ratio_ = std::clamp(ratio, 0.0, 1.0);

    // If fill ratio becomes effectively zero, convert to air.
    if (fill_ratio_ < MIN_FILL_THRESHOLD) {
        material_type_ = MaterialType::AIR;
        fill_ratio_ = 0.0;
        velocity_ = Vector2d(0.0, 0.0);
        com_ = Vector2d(0.0, 0.0);
        
        // Clear all pressure values when cell becomes empty.
        hydrostatic_pressure_ = 0.0;
        dynamic_pressure_ = 0.0;
        pressure_gradient_ = Vector2d(0.0, 0.0);
    }

    markDirty();
}

void CellB::setCOM(const Vector2d& com)
{
    com_ = Vector2d(std::clamp(com.x, COM_MIN, COM_MAX), std::clamp(com.y, COM_MIN, COM_MAX));
    markDirty(); // Ensure visual updates when COM changes.
}

double CellB::getMass() const
{
    if (isEmpty()) {
        return 0.0;
    }
    return fill_ratio_ * getMaterialDensity(material_type_);
}

double CellB::getEffectiveDensity() const
{
    return fill_ratio_ * getMaterialDensity(material_type_);
}

const MaterialProperties& CellB::getMaterialProperties() const
{
    return ::getMaterialProperties(material_type_);
}

double CellB::addMaterial(MaterialType type, double amount)
{
    if (amount <= 0.0) {
        return 0.0;
    }

    // If we're empty, accept any material type.
    if (isEmpty()) {
        material_type_ = type;
        const double added = std::min(amount, 1.0);
        fill_ratio_ = added;
        markDirty();
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
        markDirty();
    }

    return added;
}

double CellB::addMaterialWithPhysics(
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

        markDirty();
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
        const double added_mass = added * getMaterialProperties().density;
        const double total_mass = existing_mass + added_mass;

        // Calculate incoming material's COM in target cell space.
        Vector2d incoming_com = calculateTrajectoryLanding(source_com, velocity, boundary_normal);

        if (total_mass > WorldB::MIN_MATTER_THRESHOLD) {
            // Weighted average of COM positions.
            com_ = (com_ * existing_mass + incoming_com * added_mass) / total_mass;

            // Momentum conservation for velocity.
            velocity_ = (velocity_ * existing_mass + velocity * added_mass) / total_mass;
        }

        fill_ratio_ += added;
        markDirty();
    }

    return added;
}

double CellB::removeMaterial(double amount)
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

double CellB::transferTo(CellB& target, double amount)
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

double CellB::transferToWithPhysics(CellB& target, double amount, const Vector2d& boundary_normal)
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

void CellB::replaceMaterial(MaterialType type, double fill_ratio)
{
    material_type_ = type;
    setFillRatio(fill_ratio);

    // Reset physics state when replacing material.
    velocity_ = Vector2d(0.0, 0.0);
    com_ = Vector2d(0.0, 0.0);
}

void CellB::clear()
{
    material_type_ = MaterialType::AIR;
    fill_ratio_ = 0.0;
    velocity_ = Vector2d(0.0, 0.0);
    com_ = Vector2d(0.0, 0.0);
    
    // Clear all pressure values when cell becomes empty.
    hydrostatic_pressure_ = 0.0;
    dynamic_pressure_ = 0.0;
    pressure_gradient_ = Vector2d(0.0, 0.0);
    
    markDirty();
}

void CellB::limitVelocity(
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

void CellB::clampCOM()
{
    com_.x = std::clamp(com_.x, COM_MIN, COM_MAX);
    com_.y = std::clamp(com_.y, COM_MIN, COM_MAX);
}

bool CellB::shouldTransfer() const
{
    if (isEmpty() || isWall()) {
        return false;
    }

    // Transfer only when COM reaches cell boundaries (±1.0) per GridMechanics.md.
    return std::abs(com_.x) >= 1.0 || std::abs(com_.y) >= 1.0;
}

Vector2d CellB::getTransferDirection() const
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

Vector2d CellB::calculateTrajectoryLanding(
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

std::string CellB::toString() const
{
    std::ostringstream oss;
    oss << getMaterialName(material_type_) << "(fill=" << fill_ratio_ << ", com=[" << com_.x << ","
        << com_.y << "]" << ", vel=[" << velocity_.x << "," << velocity_.y << "]" << ")";
    return oss.str();
}

// =================================================================.
// CELLINTERFACE IMPLEMENTATION.
// =================================================================.

void CellB::addDirt(double amount)
{
    if (amount <= 0.0) return;
    addMaterial(MaterialType::DIRT, amount);
}

void CellB::addWater(double amount)
{
    if (amount <= 0.0) return;
    addMaterial(MaterialType::WATER, amount);
}

void CellB::addDirtWithVelocity(double amount, const Vector2d& velocity)
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

void CellB::addWaterWithVelocity(double amount, const Vector2d& velocity)
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

void CellB::addDirtWithCOM(double amount, const Vector2d& com, const Vector2d& velocity)
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

void CellB::markDirty()
{
    needs_redraw_ = true;
}

double CellB::getTotalMaterial() const
{
    return fill_ratio_;
}

// =================================================================.
// RENDERING METHODS.
// =================================================================.

void CellB::draw(lv_obj_t* parent, uint32_t x, uint32_t y)
{
    if (!needs_redraw_) {
        return; // Skip redraw if not needed.
    }

    // Use debug mode based on Cell::debugDraw static flag.
    spdlog::trace(
        "[RENDER] CellB::draw() called for cell ({},{}) - debugDraw={}, hydrostatic={}, dynamic={}",
        x,
        y,
        Cell::debugDraw,
        hydrostatic_pressure_,
        dynamic_pressure_);
    if (Cell::debugDraw) {
        spdlog::trace("[RENDER] Entering drawDebug() mode");
        drawDebug(parent, x, y);
    }
    else {
        spdlog::trace("[RENDER] Entering drawNormal() mode");
        drawNormal(parent, x, y);
    }

    needs_redraw_ = false;
}

void CellB::drawNormal(lv_obj_t* parent, uint32_t x, uint32_t y)
{
    // Create canvas if it doesn't exist.
    if (canvas_ == nullptr) {
        canvas_ = lv_canvas_create(parent);
        lv_obj_set_size(canvas_, Cell::WIDTH, Cell::HEIGHT);
        lv_obj_set_pos(canvas_, x * Cell::WIDTH, y * Cell::HEIGHT);

        // Calculate buffer size for ARGB8888 format (4 bytes per pixel).
        const size_t buffer_size = Cell::WIDTH * Cell::HEIGHT * 4;
        buffer_.resize(buffer_size);

        lv_canvas_set_buffer(
            canvas_, buffer_.data(), Cell::WIDTH, Cell::HEIGHT, LV_COLOR_FORMAT_ARGB8888);
    }

    // Zero buffer.
    std::fill(buffer_.begin(), buffer_.end(), 0);

    // Position the canvas.
    lv_obj_set_pos(canvas_, x * Cell::WIDTH, y * Cell::HEIGHT);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas_, &layer);

    // Draw black background for consistency with debug mode.
    lv_draw_rect_dsc_t bg_rect_dsc;
    lv_draw_rect_dsc_init(&bg_rect_dsc);
    bg_rect_dsc.bg_color = lv_color_hex(0x000000); // Black background.
    bg_rect_dsc.bg_opa = LV_OPA_COVER;
    bg_rect_dsc.border_width = 0;
    lv_area_t bg_coords = {
        0, 0, static_cast<int32_t>(Cell::WIDTH - 1), static_cast<int32_t>(Cell::HEIGHT - 1)
    };
    lv_draw_rect(&layer, &bg_rect_dsc, &bg_coords);

    // Render material if not empty.
    if (!isEmpty()) {
        lv_color_t material_color;

        // Use same material color mapping as debug mode.
        switch (material_type_) {
            case MaterialType::DIRT:
                material_color = lv_color_hex(0xA0522D); // Sienna brown.
                break;
            case MaterialType::WATER:
                material_color = lv_color_hex(0x00BFFF); // Deep sky blue.
                break;
            case MaterialType::WOOD:
                material_color = lv_color_hex(0xDEB887); // Burlywood.
                break;
            case MaterialType::SAND:
                material_color = lv_color_hex(0xFFB347); // Sandy orange.
                break;
            case MaterialType::METAL:
                material_color = lv_color_hex(0xC0C0C0); // Silver.
                break;
            case MaterialType::LEAF:
                material_color = lv_color_hex(0x00FF32); // Bright lime green.
                break;
            case MaterialType::WALL:
                material_color = lv_color_hex(0x808080); // Gray.
                break;
            case MaterialType::AIR:
            default:
                // Air is transparent - already has black background.
                lv_canvas_finish_layer(canvas_, &layer);
                return;
        }

        // Calculate opacity based on fill ratio.
        lv_opa_t opacity = static_cast<lv_opa_t>(fill_ratio_ * static_cast<double>(LV_OPA_COVER));

        // Draw material layer with enhanced border to match debug mode.
        lv_draw_rect_dsc_t rect_dsc;
        lv_draw_rect_dsc_init(&rect_dsc);
        rect_dsc.bg_color = material_color;
        rect_dsc.bg_opa =
            static_cast<lv_opa_t>(opacity * 0.7); // More transparent to match debug mode.
        rect_dsc.border_color = material_color;
        rect_dsc.border_opa = opacity;
        rect_dsc.border_width = 2;
        rect_dsc.radius = 2;

        // Draw full cell area.
        lv_area_t coords = {
            0, 0, static_cast<int32_t>(Cell::WIDTH - 1), static_cast<int32_t>(Cell::HEIGHT - 1)
        };

        lv_draw_rect(&layer, &rect_dsc, &coords);
    }
    // Empty cells remain transparent - no rendering needed.

    lv_canvas_finish_layer(canvas_, &layer);
}

void CellB::drawDebug(lv_obj_t* parent, uint32_t x, uint32_t y)
{
    // Create canvas if it doesn't exist.
    if (canvas_ == nullptr) {
        canvas_ = lv_canvas_create(parent);
        lv_obj_set_size(canvas_, Cell::WIDTH, Cell::HEIGHT);
        lv_obj_set_pos(canvas_, x * Cell::WIDTH, y * Cell::HEIGHT);

        // Calculate buffer size for ARGB8888 format (4 bytes per pixel).
        const size_t buffer_size = Cell::WIDTH * Cell::HEIGHT * 4;
        buffer_.resize(buffer_size);

        lv_canvas_set_buffer(
            canvas_, buffer_.data(), Cell::WIDTH, Cell::HEIGHT, LV_COLOR_FORMAT_ARGB8888);
    }

    // Zero buffer.
    std::fill(buffer_.begin(), buffer_.end(), 0);

    // Position the canvas.
    lv_obj_set_pos(canvas_, x * Cell::WIDTH, y * Cell::HEIGHT);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas_, &layer);

    // Draw black background for all cells.
    lv_draw_rect_dsc_t bg_rect_dsc;
    lv_draw_rect_dsc_init(&bg_rect_dsc);
    bg_rect_dsc.bg_color = lv_color_hex(0x000000); // Black background.
    bg_rect_dsc.bg_opa = LV_OPA_COVER;
    bg_rect_dsc.border_width = 0;
    lv_area_t coords = {
        0, 0, static_cast<int32_t>(Cell::WIDTH - 1), static_cast<int32_t>(Cell::HEIGHT - 1)
    };
    lv_draw_rect(&layer, &bg_rect_dsc, &coords);

    // Render material if not empty.
    if (!isEmpty()) {
        lv_color_t material_color;

        // Enhanced material color mapping (debug mode with brighter variants).
        switch (material_type_) {
            case MaterialType::DIRT:
                material_color = lv_color_hex(0xA0522D); // Brighter sienna brown for debug.
                break;
            case MaterialType::WATER:
                material_color = lv_color_hex(0x00BFFF); // Deep sky blue for debug.
                break;
            case MaterialType::WOOD:
                material_color = lv_color_hex(0xDEB887); // Burlywood (lighter for debug visibility).
                break;
            case MaterialType::SAND:
                material_color = lv_color_hex(0xFFB347); // Brighter sandy orange.
                break;
            case MaterialType::METAL:
                material_color = lv_color_hex(0xC0C0C0); // Silver (unchanged - already visible).
                break;
            case MaterialType::LEAF:
                material_color = lv_color_hex(0x00FF32); // Bright lime green for debug.
                break;
            case MaterialType::WALL:
                material_color = lv_color_hex(0x808080); // Gray (unchanged for solid appearance).
                break;
            case MaterialType::AIR:
            default:
                // Air gets black background only - continue to debug overlay.
                break;
        }

        if (material_type_ != MaterialType::AIR) {
            // Calculate opacity based on fill ratio.
            lv_opa_t opacity =
                static_cast<lv_opa_t>(fill_ratio_ * static_cast<double>(LV_OPA_COVER));

            // Draw material layer with enhanced border for debug.
            lv_draw_rect_dsc_t rect_dsc;
            lv_draw_rect_dsc_init(&rect_dsc);
            rect_dsc.bg_color = material_color;
            rect_dsc.bg_opa = static_cast<lv_opa_t>(opacity * 0.7); // More transparent for overlay.
            rect_dsc.border_color = material_color;
            rect_dsc.border_opa = opacity;
            rect_dsc.border_width = 2;
            rect_dsc.radius = 2;
            lv_draw_rect(&layer, &rect_dsc, &coords);
        }

        // Draw Center of Mass as yellow square.
        if (com_.x != 0.0 || com_.y != 0.0) {
            // Calculate center of mass pixel position.
            int pixel_x = static_cast<int>((com_.x + 1.0) * (Cell::WIDTH - 1) / 2.0);
            int pixel_y = static_cast<int>((com_.y + 1.0) * (Cell::HEIGHT - 1) / 2.0);

            // Draw small square centered at COM position.
            const int square_size = 6; // Size of the COM indicator square.
            const int half_size = square_size / 2;

            lv_draw_rect_dsc_t com_rect_dsc;
            lv_draw_rect_dsc_init(&com_rect_dsc);
            com_rect_dsc.bg_color = lv_color_hex(0xFFFF00); // Bright yellow.
            com_rect_dsc.bg_opa = LV_OPA_COVER;
            com_rect_dsc.border_color = lv_color_hex(0xCC9900); // Darker yellow border.
            com_rect_dsc.border_opa = LV_OPA_COVER;
            com_rect_dsc.border_width = 1;
            com_rect_dsc.radius = 0; // Sharp corners for square.

            lv_area_t com_coords = { pixel_x - half_size,
                                     pixel_y - half_size,
                                     pixel_x + half_size - 1,
                                     pixel_y + half_size - 1 };

            lv_draw_rect(&layer, &com_rect_dsc, &com_coords);
        }

        // Draw velocity vector as green line starting from COM position.
        if (velocity_.mag() > 0.01) {
            // Calculate COM pixel position (same as COM indicator calculation).
            int com_pixel_x = static_cast<int>((com_.x + 1.0) * (Cell::WIDTH - 1) / 2.0);
            int com_pixel_y = static_cast<int>((com_.y + 1.0) * (Cell::HEIGHT - 1) / 2.0);

            // Scale velocity for visualization.
            double scale = 20.0;
            int end_x = com_pixel_x + static_cast<int>(velocity_.x * scale);
            int end_y = com_pixel_y + static_cast<int>(velocity_.y * scale);

            // NO clamping - allow velocity vector to extend beyond cell bounds.
            // This shows the true trajectory and projected target location.

            lv_draw_line_dsc_t line_dsc;
            lv_draw_line_dsc_init(&line_dsc);
            line_dsc.color = lv_color_hex(0x00FF00); // Bright green.
            line_dsc.width = 2;
            line_dsc.p1.x = com_pixel_x;
            line_dsc.p1.y = com_pixel_y;
            line_dsc.p2.x = end_x;
            line_dsc.p2.y = end_y;
            lv_draw_line(&layer, &line_dsc);
        }

        // Draw force vectors from COM position.
        int com_pixel_x = static_cast<int>((com_.x + 1.0) * (Cell::WIDTH - 1) / 2.0);
        int com_pixel_y = static_cast<int>((com_.y + 1.0) * (Cell::HEIGHT - 1) / 2.0);

        // Draw cohesion force (resistance) as red upward line.
        if (accumulated_cohesion_force_.mag() > 0.01) {
            double scale = 15.0;
            int end_x = com_pixel_x + static_cast<int>(accumulated_cohesion_force_.x * scale);
            int end_y = com_pixel_y + static_cast<int>(accumulated_cohesion_force_.y * scale);

            lv_draw_line_dsc_t cohesion_line_dsc;
            lv_draw_line_dsc_init(&cohesion_line_dsc);
            cohesion_line_dsc.color = lv_color_hex(0xFF0000); // Red for cohesion resistance.
            cohesion_line_dsc.width = 2;
            cohesion_line_dsc.p1.x = com_pixel_x;
            cohesion_line_dsc.p1.y = com_pixel_y;
            cohesion_line_dsc.p2.x = end_x;
            cohesion_line_dsc.p2.y = end_y;
            lv_draw_line(&layer, &cohesion_line_dsc);
        }

        // Draw adhesion force as orange line (only if adhesion drawing is enabled).
        if (Cell::adhesionDrawEnabled && accumulated_adhesion_force_.mag() > 0.01) {
            double scale = 15.0;
            int end_x = com_pixel_x + static_cast<int>(accumulated_adhesion_force_.x * scale);
            int end_y = com_pixel_y + static_cast<int>(accumulated_adhesion_force_.y * scale);

            lv_draw_line_dsc_t adhesion_line_dsc;
            lv_draw_line_dsc_init(&adhesion_line_dsc);
            adhesion_line_dsc.color = lv_color_hex(0xFF8000); // Orange for adhesion.
            adhesion_line_dsc.width = 2;
            adhesion_line_dsc.p1.x = com_pixel_x;
            adhesion_line_dsc.p1.y = com_pixel_y;
            adhesion_line_dsc.p2.x = end_x;
            adhesion_line_dsc.p2.y = end_y;
            lv_draw_line(&layer, &adhesion_line_dsc);
        }

        // Draw COM cohesion force as cyan line.
        if (accumulated_com_cohesion_force_.mag() > 0.01) {
            double scale = 15.0;
            int end_x = com_pixel_x + static_cast<int>(accumulated_com_cohesion_force_.x * scale);
            int end_y = com_pixel_y + static_cast<int>(accumulated_com_cohesion_force_.y * scale);

            lv_draw_line_dsc_t com_cohesion_line_dsc;
            lv_draw_line_dsc_init(&com_cohesion_line_dsc);
            com_cohesion_line_dsc.color = lv_color_hex(0x00FFFF); // Cyan for COM cohesion.
            com_cohesion_line_dsc.width = 2;
            com_cohesion_line_dsc.p1.x = com_pixel_x;
            com_cohesion_line_dsc.p1.y = com_pixel_y;
            com_cohesion_line_dsc.p2.x = end_x;
            com_cohesion_line_dsc.p2.y = end_y;
            lv_draw_line(&layer, &com_cohesion_line_dsc);
        }

        // Draw pressure visualization as dual-layer border.
        // Inner border: hydrostatic pressure (red).
        // Outer border: dynamic pressure (magenta).
        
        // Calculate border widths based on pressure values.
        // Use log scale for better visualization of pressure ranges.
        const double MAX_BORDER_WIDTH = 8.0; // Maximum border width in pixels.
        
        int hydrostatic_border_width = 0;
        int dynamic_border_width = 0;
        
        if (hydrostatic_pressure_ > 0.01) {
            // Map hydrostatic pressure to border width (1-8 pixels).
            hydrostatic_border_width = static_cast<int>(
                std::min(MAX_BORDER_WIDTH, 1.0 + std::log1p(hydrostatic_pressure_ * 10) * 2.0));
        }
        
        if (dynamic_pressure_ > 0.01) {
            // Map dynamic pressure to border width (1-8 pixels).
            dynamic_border_width = static_cast<int>(
                std::min(MAX_BORDER_WIDTH, 1.0 + std::log1p(dynamic_pressure_ * 10) * 2.0));
        }
        
        spdlog::trace(
            "[RENDER DEBUG] Cell hydrostatic_pressure_={}, dynamic_pressure_={}, "
            "hydrostatic_border_width={}, dynamic_border_width={}",
            hydrostatic_pressure_,
            dynamic_pressure_,
            hydrostatic_border_width,
            dynamic_border_width);
        
        // Draw outer border first (dynamic pressure - magenta).
        if (dynamic_border_width > 0) {
            lv_draw_rect_dsc_t dynamic_border_dsc;
            lv_draw_rect_dsc_init(&dynamic_border_dsc);
            dynamic_border_dsc.bg_opa = LV_OPA_TRANSP; // Transparent background.
            dynamic_border_dsc.border_color = lv_color_hex(0xFF00FF); // Magenta.
            dynamic_border_dsc.border_opa = LV_OPA_COVER;
            dynamic_border_dsc.border_width = dynamic_border_width;
            dynamic_border_dsc.radius = 0;
            
            lv_area_t dynamic_border_area = coords; // Full cell area.
            lv_draw_rect(&layer, &dynamic_border_dsc, &dynamic_border_area);
        }
        
        // Draw inner border (hydrostatic pressure - red).
        if (hydrostatic_border_width > 0) {
            lv_draw_rect_dsc_t hydrostatic_border_dsc;
            lv_draw_rect_dsc_init(&hydrostatic_border_dsc);
            hydrostatic_border_dsc.bg_opa = LV_OPA_TRANSP; // Transparent background.
            hydrostatic_border_dsc.border_color = lv_color_hex(0xFF0000); // Red.
            hydrostatic_border_dsc.border_opa = LV_OPA_COVER;
            hydrostatic_border_dsc.border_width = hydrostatic_border_width;
            hydrostatic_border_dsc.radius = 0;
            
            // Inset the hydrostatic border by the dynamic border width.
            lv_area_t hydrostatic_border_area;
            hydrostatic_border_area.x1 = dynamic_border_width;
            hydrostatic_border_area.y1 = dynamic_border_width;
            hydrostatic_border_area.x2 = Cell::WIDTH - 1 - dynamic_border_width;
            hydrostatic_border_area.y2 = Cell::HEIGHT - 1 - dynamic_border_width;
            lv_draw_rect(&layer, &hydrostatic_border_dsc, &hydrostatic_border_area);
        }
        
        // Draw pressure gradient vector as magenta line.
        if (pressure_gradient_.magnitude() > 0.001) {
            // Calculate center position.
            int center_x = Cell::WIDTH / 2;
            int center_y = Cell::HEIGHT / 2;
            
            // Scale factor for visualization.
            const double GRADIENT_SCALE = 30.0;
            
            // Calculate end point of gradient vector (reversed to show flow direction).
            int end_x = center_x - static_cast<int>(pressure_gradient_.x * GRADIENT_SCALE);
            int end_y = center_y - static_cast<int>(pressure_gradient_.y * GRADIENT_SCALE);
            
            // Draw gradient line.
            lv_draw_line_dsc_t gradient_line_dsc;
            lv_draw_line_dsc_init(&gradient_line_dsc);
            gradient_line_dsc.color = lv_color_hex(0xFF0080); // Magenta for pressure gradient.
            gradient_line_dsc.width = 3;
            gradient_line_dsc.opa = LV_OPA_COVER;
            gradient_line_dsc.p1.x = center_x;
            gradient_line_dsc.p1.y = center_y;
            gradient_line_dsc.p2.x = end_x;
            gradient_line_dsc.p2.y = end_y;
            lv_draw_line(&layer, &gradient_line_dsc);
            
            // Add arrowhead for gradient direction.
            if (pressure_gradient_.magnitude() > 0.01) {
                double angle = atan2(-pressure_gradient_.y, -pressure_gradient_.x);
                int arrow_len = 6;
                
                lv_draw_line_dsc_t arrow_dsc = gradient_line_dsc;
                arrow_dsc.width = 2;
                
                // Left arrowhead line.
                arrow_dsc.p1.x = end_x;
                arrow_dsc.p1.y = end_y;
                arrow_dsc.p2.x = end_x - arrow_len * cos(angle - M_PI / 6);
                arrow_dsc.p2.y = end_y - arrow_len * sin(angle - M_PI / 6);
                lv_draw_line(&layer, &arrow_dsc);
                
                // Right arrowhead line.
                arrow_dsc.p2.x = end_x - arrow_len * cos(angle + M_PI / 6);
                arrow_dsc.p2.y = end_y - arrow_len * sin(angle + M_PI / 6);
                lv_draw_line(&layer, &arrow_dsc);
            }
        }
    }

    lv_canvas_finish_layer(canvas_, &layer);
}

std::string CellB::toAsciiCharacter() const
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

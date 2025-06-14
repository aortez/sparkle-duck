#include "CellB.h"
#include "Cell.h" // For WIDTH/HEIGHT constants

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>

#include "lvgl/lvgl.h"

CellB::CellB()
    : material_type_(MaterialType::AIR),
      fill_ratio_(0.0),
      com_(0.0, 0.0),
      velocity_(0.0, 0.0),
      pressure_(0.0),
      canvas_(nullptr),
      needs_redraw_(true)
{
}

CellB::CellB(MaterialType type, double fill)
    : material_type_(type),
      fill_ratio_(std::clamp(fill, 0.0, 1.0)),
      com_(0.0, 0.0),
      velocity_(0.0, 0.0),
      pressure_(0.0),
      canvas_(nullptr),
      needs_redraw_(true)
{
}

CellB::~CellB()
{
    // Clean up the LVGL canvas object if it exists
    if (canvas_ != nullptr) {
        lv_obj_del(canvas_);
        canvas_ = nullptr;
    }
}

// Copy constructor - don't copy LVGL objects, they'll be recreated on demand
CellB::CellB(const CellB& other)
    : material_type_(other.material_type_),
      fill_ratio_(other.fill_ratio_),
      com_(other.com_),
      velocity_(other.velocity_),
      pressure_(other.pressure_),
      buffer_(other.buffer_.size()), // Create new buffer with same size
      canvas_(nullptr),              // Don't copy LVGL object
      needs_redraw_(true)            // New copy needs redraw
{
    // Buffer contents will be regenerated when drawn
}

// Assignment operator - don't copy LVGL objects, they'll be recreated on demand  
CellB& CellB::operator=(const CellB& other)
{
    if (this != &other) {
        // Clean up existing canvas before assignment
        if (canvas_ != nullptr) {
            lv_obj_del(canvas_);
            canvas_ = nullptr;
        }
        
        // Copy the physics state
        material_type_ = other.material_type_;
        fill_ratio_ = other.fill_ratio_;
        com_ = other.com_;
        velocity_ = other.velocity_;
        pressure_ = other.pressure_;
        
        // Resize buffer if needed but don't copy contents
        buffer_.resize(other.buffer_.size());
        
        // Don't copy LVGL object - keep canvas_ as nullptr
        // canvas_ stays as nullptr
        needs_redraw_ = true; // Assignment means we need redraw
    }
    return *this;
}

void CellB::setFillRatio(double ratio)
{
    fill_ratio_ = std::clamp(ratio, 0.0, 1.0);
    
    // If fill ratio becomes effectively zero, convert to air
    if (fill_ratio_ < MIN_FILL_THRESHOLD) {
        material_type_ = MaterialType::AIR;
        fill_ratio_ = 0.0;
        velocity_ = Vector2d(0.0, 0.0);
        com_ = Vector2d(0.0, 0.0);
    }
    
    markDirty();
}

void CellB::setCOM(const Vector2d& com)
{
    com_ = Vector2d(
        std::clamp(com.x, COM_MIN, COM_MAX),
        std::clamp(com.y, COM_MIN, COM_MAX)
    );
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
    
    // If we're empty, accept any material type
    if (isEmpty()) {
        material_type_ = type;
        const double added = std::min(amount, 1.0);
        fill_ratio_ = added;
        markDirty();
        return added;
    }
    
    // If different material type, no mixing allowed
    if (material_type_ != type) {
        return 0.0;
    }
    
    // Add to existing material
    const double capacity = getCapacity();
    const double added = std::min(amount, capacity);
    fill_ratio_ += added;
    
    if (added > 0.0) {
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
    
    // Check if we became empty
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
    
    // Calculate how much we can actually transfer
    const double available = std::min(amount, fill_ratio_);
    const double accepted = target.addMaterial(material_type_, available);
    
    // Remove the accepted amount from this cell
    if (accepted > 0.0) {
        removeMaterial(accepted);
    }
    
    return accepted;
}

void CellB::replaceMaterial(MaterialType type, double fill_ratio)
{
    material_type_ = type;
    setFillRatio(fill_ratio);
    
    // Reset physics state when replacing material
    velocity_ = Vector2d(0.0, 0.0);
    com_ = Vector2d(0.0, 0.0);
    pressure_ = 0.0;
}

void CellB::clear()
{
    material_type_ = MaterialType::AIR;
    fill_ratio_ = 0.0;
    velocity_ = Vector2d(0.0, 0.0);
    com_ = Vector2d(0.0, 0.0);
    pressure_ = 0.0;
    markDirty();
}

void CellB::limitVelocity(double max_velocity, double damping_threshold, double damping_factor)
{
    const double speed = velocity_.mag();
    
    // Apply maximum velocity limit
    if (speed > max_velocity) {
        velocity_ = velocity_ * (max_velocity / speed);
    }
    
    // Apply damping when above threshold
    if (speed > damping_threshold) {
        velocity_ = velocity_ * (1.0 - damping_factor);
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
    
    // Transfer if COM is outside the center region
    return std::abs(com_.x) > 0.5 || std::abs(com_.y) > 0.5;
}

Vector2d CellB::getTransferDirection() const
{
    // Determine primary transfer direction based on COM position
    Vector2d direction(0.0, 0.0);
    
    if (com_.x > 0.5) {
        direction.x = 1.0;  // Transfer right
    } else if (com_.x < -0.5) {
        direction.x = -1.0; // Transfer left
    }
    
    if (com_.y > 0.5) {
        direction.y = 1.0;  // Transfer down
    } else if (com_.y < -0.5) {
        direction.y = -1.0; // Transfer up
    }
    
    return direction;
}

std::string CellB::toString() const
{
    std::ostringstream oss;
    oss << getMaterialName(material_type_) 
        << "(fill=" << fill_ratio_
        << ", com=[" << com_.x << "," << com_.y << "]"
        << ", vel=[" << velocity_.x << "," << velocity_.y << "]"
        << ", p=" << pressure_ << ")";
    return oss.str();
}

// =================================================================
// CELLINTERFACE IMPLEMENTATION
// =================================================================

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
    
    // Store current fill ratio to calculate momentum
    double oldFill = fill_ratio_;
    double actualAdded = addMaterial(MaterialType::DIRT, amount);
    
    if (actualAdded > 0.0) {
        // Update velocity based on momentum conservation
        double newFill = fill_ratio_;
        if (newFill > 0.0) {
            // Weighted average of existing velocity and new velocity
            velocity_ = (velocity_ * oldFill + velocity * actualAdded) / newFill;
        } else {
            velocity_ = velocity;
        }
    }
}

void CellB::addWaterWithVelocity(double amount, const Vector2d& velocity)
{
    if (amount <= 0.0) return;
    
    // Store current fill ratio to calculate momentum
    double oldFill = fill_ratio_;
    double actualAdded = addMaterial(MaterialType::WATER, amount);
    
    if (actualAdded > 0.0) {
        // Update velocity based on momentum conservation
        double newFill = fill_ratio_;
        if (newFill > 0.0) {
            // Weighted average of existing velocity and new velocity
            velocity_ = (velocity_ * oldFill + velocity * actualAdded) / newFill;
        } else {
            velocity_ = velocity;
        }
    }
}

void CellB::addDirtWithCOM(double amount, const Vector2d& com, const Vector2d& velocity)
{
    if (amount <= 0.0) return;
    
    // Store current state to calculate weighted averages
    double oldFill = fill_ratio_;
    Vector2d oldCOM = com_;
    Vector2d oldVelocity = velocity_;
    
    double actualAdded = addMaterial(MaterialType::DIRT, amount);
    
    if (actualAdded > 0.0) {
        double newFill = fill_ratio_;
        if (newFill > 0.0) {
            // Weighted average of existing COM and new COM
            com_ = (oldCOM * oldFill + com * actualAdded) / newFill;
            clampCOM(); // Ensure COM stays in bounds
            
            // Weighted average of existing velocity and new velocity
            velocity_ = (oldVelocity * oldFill + velocity * actualAdded) / newFill;
        } else {
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

// =================================================================
// RENDERING METHODS
// =================================================================

void CellB::draw(lv_obj_t* parent, uint32_t x, uint32_t y)
{
    if (!needs_redraw_) {
        return; // Skip redraw if not needed
    }
    
    // Use debug mode based on Cell::debugDraw static flag
    if (Cell::debugDraw) {
        drawDebug(parent, x, y);
    } else {
        drawNormal(parent, x, y);
    }
    
    needs_redraw_ = false;
}

void CellB::drawNormal(lv_obj_t* parent, uint32_t x, uint32_t y)
{
    // Create canvas if it doesn't exist
    if (canvas_ == nullptr) {
        canvas_ = lv_canvas_create(parent);
        lv_obj_set_size(canvas_, Cell::WIDTH, Cell::HEIGHT);
        lv_obj_set_pos(canvas_, x * Cell::WIDTH, y * Cell::HEIGHT);
        
        // Calculate buffer size for ARGB8888 format (4 bytes per pixel)
        const size_t buffer_size = Cell::WIDTH * Cell::HEIGHT * 4;
        buffer_.resize(buffer_size);
        
        lv_canvas_set_buffer(canvas_, buffer_.data(), Cell::WIDTH, Cell::HEIGHT, LV_COLOR_FORMAT_ARGB8888);
    }
    
    // Zero buffer
    std::fill(buffer_.begin(), buffer_.end(), 0);
    
    // Position the canvas
    lv_obj_set_pos(canvas_, x * Cell::WIDTH, y * Cell::HEIGHT);
    
    lv_layer_t layer;
    lv_canvas_init_layer(canvas_, &layer);
    
    // Draw black background for all cells
    lv_draw_rect_dsc_t bg_rect_dsc;
    lv_draw_rect_dsc_init(&bg_rect_dsc);
    bg_rect_dsc.bg_color = lv_color_hex(0x000000); // Black background
    bg_rect_dsc.bg_opa = LV_OPA_COVER;
    bg_rect_dsc.border_width = 0;
    lv_area_t coords = { 0, 0, Cell::WIDTH, Cell::HEIGHT };
    lv_draw_rect(&layer, &bg_rect_dsc, &coords);
    
    // Render material if not empty
    if (!isEmpty()) {
        lv_color_t material_color;
        
        // Map material types to colors
        switch (material_type_) {
            case MaterialType::DIRT:
                material_color = lv_color_hex(0x8B4513); // Saddle brown
                break;
            case MaterialType::WATER:
                material_color = lv_color_hex(0x0000FF); // Blue
                break;
            case MaterialType::WOOD:
                material_color = lv_color_hex(0x8B7355); // Dark khaki
                break;
            case MaterialType::SAND:
                material_color = lv_color_hex(0xF4A460); // Sandy brown
                break;
            case MaterialType::METAL:
                material_color = lv_color_hex(0xC0C0C0); // Silver
                break;
            case MaterialType::LEAF:
                material_color = lv_color_hex(0x228B22); // Forest green
                break;
            case MaterialType::WALL:
                material_color = lv_color_hex(0x808080); // Gray
                break;
            case MaterialType::AIR:
            default:
                // Air gets black background only
                lv_canvas_finish_layer(canvas_, &layer);
                return;
        }
        
        // Calculate opacity based on fill ratio
        lv_opa_t opacity = static_cast<lv_opa_t>(fill_ratio_ * LV_OPA_COVER);
        
        // Draw material layer
        lv_draw_rect_dsc_t rect_dsc;
        lv_draw_rect_dsc_init(&rect_dsc);
        rect_dsc.bg_color = material_color;
        rect_dsc.bg_opa = opacity;
        rect_dsc.border_color = material_color; // Same color as background
        rect_dsc.border_opa = static_cast<lv_opa_t>(opacity * 0.3); // 30% of opacity for softer look
        rect_dsc.border_width = 1;
        rect_dsc.radius = 1;
        lv_draw_rect(&layer, &rect_dsc, &coords);
    }
    
    lv_canvas_finish_layer(canvas_, &layer);
}

void CellB::drawDebug(lv_obj_t* parent, uint32_t x, uint32_t y)
{
    // Create canvas if it doesn't exist
    if (canvas_ == nullptr) {
        canvas_ = lv_canvas_create(parent);
        lv_obj_set_size(canvas_, Cell::WIDTH, Cell::HEIGHT);
        lv_obj_set_pos(canvas_, x * Cell::WIDTH, y * Cell::HEIGHT);
        
        // Calculate buffer size for ARGB8888 format (4 bytes per pixel)
        const size_t buffer_size = Cell::WIDTH * Cell::HEIGHT * 4;
        buffer_.resize(buffer_size);
        
        lv_canvas_set_buffer(canvas_, buffer_.data(), Cell::WIDTH, Cell::HEIGHT, LV_COLOR_FORMAT_ARGB8888);
    }
    
    // Zero buffer
    std::fill(buffer_.begin(), buffer_.end(), 0);
    
    // Position the canvas
    lv_obj_set_pos(canvas_, x * Cell::WIDTH, y * Cell::HEIGHT);
    
    lv_layer_t layer;
    lv_canvas_init_layer(canvas_, &layer);
    
    // Draw black background for all cells
    lv_draw_rect_dsc_t bg_rect_dsc;
    lv_draw_rect_dsc_init(&bg_rect_dsc);
    bg_rect_dsc.bg_color = lv_color_hex(0x000000); // Black background
    bg_rect_dsc.bg_opa = LV_OPA_COVER;
    bg_rect_dsc.border_width = 0;
    lv_area_t coords = { 0, 0, Cell::WIDTH, Cell::HEIGHT };
    lv_draw_rect(&layer, &bg_rect_dsc, &coords);
    
    // Render material if not empty
    if (!isEmpty()) {
        lv_color_t material_color;
        
        // Map material types to colors (enhanced for debug)
        switch (material_type_) {
            case MaterialType::DIRT:
                material_color = lv_color_hex(0x8B4513); // Saddle brown
                break;
            case MaterialType::WATER:
                material_color = lv_color_hex(0x0066FF); // Brighter blue for debug
                break;
            case MaterialType::WOOD:
                material_color = lv_color_hex(0x8B7355); // Dark khaki
                break;
            case MaterialType::SAND:
                material_color = lv_color_hex(0xF4A460); // Sandy brown
                break;
            case MaterialType::METAL:
                material_color = lv_color_hex(0xC0C0C0); // Silver
                break;
            case MaterialType::LEAF:
                material_color = lv_color_hex(0x228B22); // Forest green
                break;
            case MaterialType::WALL:
                material_color = lv_color_hex(0x808080); // Gray
                break;
            case MaterialType::AIR:
            default:
                // Air gets black background only - continue to debug overlay
                break;
        }
        
        if (material_type_ != MaterialType::AIR) {
            // Calculate opacity based on fill ratio
            lv_opa_t opacity = static_cast<lv_opa_t>(fill_ratio_ * LV_OPA_COVER);
            
            // Draw material layer with enhanced border for debug
            lv_draw_rect_dsc_t rect_dsc;
            lv_draw_rect_dsc_init(&rect_dsc);
            rect_dsc.bg_color = material_color;
            rect_dsc.bg_opa = static_cast<lv_opa_t>(opacity * 0.7); // More transparent for overlay
            rect_dsc.border_color = material_color;
            rect_dsc.border_opa = opacity;
            rect_dsc.border_width = 2;
            rect_dsc.radius = 2;
            lv_draw_rect(&layer, &rect_dsc, &coords);
        }
    
        // Draw Center of Mass as yellow circle
        if (com_.x != 0.0 || com_.y != 0.0) {
            // Calculate center of mass pixel position
            int pixel_x = static_cast<int>((com_.x + 1.0) * (Cell::WIDTH - 1) / 2.0);
            int pixel_y = static_cast<int>((com_.y + 1.0) * (Cell::HEIGHT - 1) / 2.0);
            
            lv_draw_arc_dsc_t arc_dsc;
            lv_draw_arc_dsc_init(&arc_dsc);
            arc_dsc.color = lv_color_hex(0xFFFF00); // Bright yellow
            arc_dsc.center.x = pixel_x;
            arc_dsc.center.y = pixel_y;
            arc_dsc.width = 3;
            arc_dsc.start_angle = 0;
            arc_dsc.end_angle = 360;
            arc_dsc.radius = 8;
            lv_draw_arc(&layer, &arc_dsc);
        }
        
        // Draw velocity vector as green line
        if (velocity_.mag() > 0.01) {
            int center_x = Cell::WIDTH / 2;
            int center_y = Cell::HEIGHT / 2;
            
            // Scale velocity for visualization
            double scale = 20.0;
            int end_x = center_x + static_cast<int>(velocity_.x * scale);
            int end_y = center_y + static_cast<int>(velocity_.y * scale);
            
            // Clamp to canvas bounds
            end_x = std::clamp(end_x, 5, static_cast<int>(Cell::WIDTH - 5));
            end_y = std::clamp(end_y, 5, static_cast<int>(Cell::HEIGHT - 5));
            
            lv_draw_line_dsc_t line_dsc;
            lv_draw_line_dsc_init(&line_dsc);
            line_dsc.color = lv_color_hex(0x00FF00); // Bright green
            line_dsc.width = 2;
            line_dsc.p1.x = center_x;
            line_dsc.p1.y = center_y;
            line_dsc.p2.x = end_x;
            line_dsc.p2.y = end_y;
            lv_draw_line(&layer, &line_dsc);
        }
    }
    
    lv_canvas_finish_layer(canvas_, &layer);
}
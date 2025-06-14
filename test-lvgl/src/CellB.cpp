#include "CellB.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>

CellB::CellB()
    : material_type_(MaterialType::AIR),
      fill_ratio_(0.0),
      com_(0.0, 0.0),
      velocity_(0.0, 0.0),
      pressure_(0.0)
{
}

CellB::CellB(MaterialType type, double fill)
    : material_type_(type),
      fill_ratio_(std::clamp(fill, 0.0, 1.0)),
      com_(0.0, 0.0),
      velocity_(0.0, 0.0),
      pressure_(0.0)
{
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
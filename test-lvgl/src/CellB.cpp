#include "CellB.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cmath>
#include <sstream>

// Static member definitions
bool CellB::debugDraw = false;
uint32_t CellB::WIDTH = 4;
uint32_t CellB::HEIGHT = 4;
std::unordered_map<MaterialType, MaterialProperties> CellB::materialProperties_;
bool CellB::propertiesInitialized_ = false;

CellB::CellB() 
    : fill_ratio(0.0)
    , material(MaterialType::AIR)
    , com(0.0, 0.0)
    , v(0.0, 0.0)
    , pressure(0.0, 0.0)
{
    if (!propertiesInitialized_) {
        initializeMaterialProperties();
    }
}

CellB::~CellB() = default;

CellB::CellB(const CellB& other)
    : fill_ratio(other.fill_ratio)
    , material(other.material)
    , com(other.com)
    , v(other.v)
    , pressure(other.pressure)
{
}

CellB& CellB::operator=(const CellB& other)
{
    if (this != &other) {
        fill_ratio = other.fill_ratio;
        material = other.material;
        com = other.com;
        v = other.v;
        pressure = other.pressure;
    }
    return *this;
}

void CellB::initializeMaterialProperties()
{
    // Initialize material properties based on design document
    materialProperties_[MaterialType::AIR] = MaterialProperties(0.0, 1.0, 0.0, 0.0);
    materialProperties_[MaterialType::DIRT] = MaterialProperties(1.3, 0.6, 0.7, 0.4);
    materialProperties_[MaterialType::WATER] = MaterialProperties(1.0, 0.1, 0.3, 0.8);
    materialProperties_[MaterialType::WOOD] = MaterialProperties(0.8, 0.9, 0.8, 0.3);
    materialProperties_[MaterialType::SAND] = MaterialProperties(1.5, 0.4, 0.5, 0.2);
    materialProperties_[MaterialType::METAL] = MaterialProperties(2.0, 0.95, 0.9, 0.1);
    materialProperties_[MaterialType::LEAF] = MaterialProperties(0.7, 0.3, 0.6, 0.5);
    materialProperties_[MaterialType::WALL] = MaterialProperties(3.0, 1.0, 1.0, 0.0);
    
    propertiesInitialized_ = true;
    spdlog::debug("Initialized CellB material properties");
}

const MaterialProperties& CellB::getMaterialProperties(MaterialType type)
{
    if (!propertiesInitialized_) {
        initializeMaterialProperties();
    }
    return materialProperties_.at(type);
}

Vector2d CellB::getNormalizedDeflection() const
{
    return Vector2d(
        com.x / COM_DEFLECTION_THRESHOLD,
        com.y / COM_DEFLECTION_THRESHOLD
    );
}

double CellB::getDensity() const
{
    return getMaterialProperties(material).density;
}

double CellB::getElasticity() const
{
    return getMaterialProperties(material).elasticity;
}

void CellB::setMaterial(MaterialType newMaterial, double newFillRatio)
{
    material = newMaterial;
    fill_ratio = std::clamp(newFillRatio, 0.0, 1.0);
    
    // If setting to AIR, reset fill ratio to 0
    if (material == MaterialType::AIR) {
        fill_ratio = 0.0;
    }
}

double CellB::addMaterial(MaterialType materialType, double amount)
{
    if (amount <= 0.0) return 0.0;
    
    // If cell is empty, set the material type
    if (isEmpty()) {
        material = materialType;
        fill_ratio = 0.0;
    }
    
    // Can only add material of the same type
    if (material != materialType && !isEmpty()) {
        spdlog::warn("Attempted to add {} to cell containing different material", 
                     static_cast<int>(materialType));
        return 0.0;
    }
    
    // Calculate how much we can actually add
    double availableSpace = 1.0 - fill_ratio;
    double actualAmount = std::min(amount, availableSpace);
    
    fill_ratio += actualAmount;
    fill_ratio = std::clamp(fill_ratio, 0.0, 1.0);
    
    return actualAmount;
}

double CellB::removeMaterial(double amount)
{
    if (amount <= 0.0 || isEmpty()) return 0.0;
    
    double actualAmount = std::min(amount, fill_ratio);
    fill_ratio -= actualAmount;
    fill_ratio = std::max(fill_ratio, 0.0);
    
    // If cell becomes empty, set material to AIR
    if (fill_ratio <= 0.0) {
        material = MaterialType::AIR;
        fill_ratio = 0.0;
    }
    
    return actualAmount;
}

void CellB::validateState(const std::string& context) const
{
    if (fill_ratio < 0.0 || fill_ratio > 1.0) {
        spdlog::error("Invalid fill_ratio {} in context: {}", fill_ratio, context);
    }
    
    if (material == MaterialType::AIR && fill_ratio > 0.0) {
        spdlog::error("AIR material with non-zero fill_ratio {} in context: {}", fill_ratio, context);
    }
    
    if (fill_ratio <= 0.0 && material != MaterialType::AIR) {
        spdlog::error("Zero fill_ratio with non-AIR material in context: {}", context);
    }
    
    if (std::abs(com.x) > 1.0 || std::abs(com.y) > 1.0) {
        spdlog::warn("COM out of bounds [{}, {}] in context: {}", com.x, com.y, context);
    }
}

std::string CellB::toString() const
{
    std::stringstream ss;
    ss << "CellB{";
    ss << "material=" << static_cast<int>(material);
    ss << ", fill=" << fill_ratio;
    ss << ", com=(" << com.x << "," << com.y << ")";
    ss << ", v=(" << v.x << "," << v.y << ")";
    ss << "}";
    return ss.str();
}
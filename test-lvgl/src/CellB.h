#pragma once

#include "Vector2d.h"

#include <cstdint>
#include <string>
#include <unordered_map>

// Forward declare LVGL types.
typedef struct _lv_obj_t lv_obj_t;

// Forward declare World class
class World;

// Material types for WorldRulesB cells
enum class MaterialType {
    AIR = 0,    // Empty space
    DIRT,       // Granular solid material
    WATER,      // Fluid material
    WOOD,       // Rigid solid material
    SAND,       // Granular solid material
    METAL,      // Very rigid solid material
    LEAF,       // Light organic material
    WALL        // Immobile boundary material
};

// Material properties structure
struct MaterialProperties {
    double density;
    double elasticity;
    double cohesion;
    double adhesion;
    
    MaterialProperties(double d = 1.0, double e = 0.8, double c = 0.5, double a = 0.3) 
        : density(d), elasticity(e), cohesion(c), adhesion(a) {}
};

// Cell implementation for WorldRulesB - uses fill ratio and pure materials
class CellB {
public:
    static bool debugDraw;
    static uint32_t WIDTH;
    static uint32_t HEIGHT;

    // COM deflection threshold for triggering transfers
    static constexpr double COM_DEFLECTION_THRESHOLD = 0.6;

    static void setSize(uint32_t newSize)
    {
        WIDTH = newSize;
        HEIGHT = newSize;
    }

    static uint32_t getSize() { return WIDTH; }

    // Get material properties for a given material type
    static const MaterialProperties& getMaterialProperties(MaterialType type);

    CellB();
    ~CellB();

    // Copy constructor and assignment operator
    CellB(const CellB& other);
    CellB& operator=(const CellB& other);

    // Fill ratio from [0,1] - how full the cell is
    double fill_ratio;
    
    // What type of material fills the cell
    MaterialType material;

    // Center of mass of matter in cell, range [-1,1]
    Vector2d com;

    // Velocity of matter in cell
    Vector2d v;

    // Pressure force vector
    Vector2d pressure;

    // Calculate how full the cell is (alias for fill_ratio for compatibility)
    double percentFull() const { return fill_ratio; }

    // Get normalized COM deflection in range [-1, 1]
    Vector2d getNormalizedDeflection() const;

    // Get density of the material in this cell
    double getDensity() const;

    // Get elasticity of the material in this cell
    double getElasticity() const;

    // Check if cell is empty
    bool isEmpty() const { return material == MaterialType::AIR || fill_ratio <= 0.0; }

    // Check if cell is wall (immobile boundary)
    bool isWall() const { return material == MaterialType::WALL; }

    // Safe method to set material and fill ratio
    void setMaterial(MaterialType newMaterial, double newFillRatio);

    // Add material to this cell (for transfers)
    // Returns actual amount added
    double addMaterial(MaterialType materialType, double amount);

    // Remove material from this cell (for transfers) 
    // Returns actual amount removed
    double removeMaterial(double amount);

    // Validate cell state for debugging
    void validateState(const std::string& context) const;

    std::string toString() const;

private:
    // Static material properties lookup table
    static std::unordered_map<MaterialType, MaterialProperties> materialProperties_;
    static bool propertiesInitialized_;
    static void initializeMaterialProperties();
};
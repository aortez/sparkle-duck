#ifndef WORLDSTATE_H
#define WORLDSTATE_H

#include <vector>
#include "Vector2d.h"
#include "MaterialType.h"
#include "lvgl/src/libs/thorvg/rapidjson/document.h"

/**
 * WorldState - State transfer structure for cross-world compatibility
 * 
 * Enables switching between World (RulesA) and WorldB (RulesB) by preserving
 * compatible state information. Uses lossy but reasonable conversion between
 * mixed-material and pure-material systems.
 */
struct WorldState {
    // Grid dimensions
    uint32_t width;
    uint32_t height;
    
    // Physics simulation state
    double timescale;
    uint32_t timestep;
    
    // Physics parameters
    double gravity;
    double elasticity_factor;
    double pressure_scale;
    double dirt_fragmentation_factor;
    
    // Water physics parameters
    double water_pressure_threshold;
    
    // World setup flags
    bool left_throw_enabled;
    bool right_throw_enabled;
    bool lower_right_quadrant_enabled;
    bool walls_enabled;
    double rain_rate;
    
    // Time reversal state
    bool time_reversal_enabled;
    
    // Other control flags
    bool add_particles_enabled;
    bool cursor_force_enabled;
    
    /**
     * CellData - Basic material data for cross-compatibility
     * 
     * Simplified representation that can be converted between Cell and CellB
     * formats with reasonable accuracy.
     */
    struct CellData {
        double material_mass;           // Total mass regardless of type
        MaterialType dominant_material; // Primary material for conversion
        Vector2d velocity;              // Cell velocity
        Vector2d com;                   // Center of mass offset
        
        // Default constructor
        CellData() : material_mass(0.0), dominant_material(MaterialType::AIR), 
                    velocity(0.0, 0.0), com(0.0, 0.0) {}
        
        // Constructor with values
        CellData(double mass, MaterialType material, const Vector2d& vel = Vector2d(0.0, 0.0),
                const Vector2d& center_of_mass = Vector2d(0.0, 0.0))
            : material_mass(mass), dominant_material(material), velocity(vel), 
              com(center_of_mass) {}
              
        // JSON serialization support
        rapidjson::Value toJson(rapidjson::Document::AllocatorType& allocator) const;
        static CellData fromJson(const rapidjson::Value& json);
    };
    
    // Grid data (row-major order: grid_data[y][x])
    std::vector<std::vector<CellData>> grid_data;
    
    // Default constructor
    WorldState() : width(0), height(0), timescale(1.0), timestep(0), 
                  gravity(9.81), elasticity_factor(0.5), pressure_scale(1.0),
                  dirt_fragmentation_factor(1.0), water_pressure_threshold(0.1),
                  left_throw_enabled(false), right_throw_enabled(false),
                  lower_right_quadrant_enabled(false), walls_enabled(true),
                  rain_rate(0.0), time_reversal_enabled(false),
                  add_particles_enabled(true), cursor_force_enabled(false) {}
    
    // Constructor with dimensions
    WorldState(uint32_t w, uint32_t h) : WorldState() {
        width = w;
        height = h;
        grid_data.resize(height, std::vector<CellData>(width));
    }
    
    /**
     * Initialize grid data with specified dimensions
     */
    void initializeGrid(uint32_t w, uint32_t h) {
        width = w;
        height = h;
        grid_data.clear();
        grid_data.resize(height, std::vector<CellData>(width));
    }
    
    /**
     * Get cell data at specified coordinates
     */
    const CellData& getCellData(uint32_t x, uint32_t y) const {
        return grid_data[y][x];
    }
    
    /**
     * Set cell data at specified coordinates
     */
    void setCellData(uint32_t x, uint32_t y, const CellData& data) {
        grid_data[y][x] = data;
    }
    
    /**
     * Check if coordinates are valid
     */
    bool isValidCoordinate(uint32_t x, uint32_t y) const {
        return x < width && y < height;
    }
    
    // JSON serialization support
    rapidjson::Value toJson(rapidjson::Document::AllocatorType& allocator) const;
    static WorldState fromJson(const rapidjson::Value& json);
};

#endif // WORLDSTATE_H
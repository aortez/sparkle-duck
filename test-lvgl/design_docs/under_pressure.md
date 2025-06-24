# Under Pressure: Implementation Plan (Updated)

## Overview

Pressure in WorldB combines two sources:
1. **Hydrostatic Pressure**: From gravity and material weight (physics-based)
2. **Dynamic Pressure**: From blocked transfers and movement resistance (interaction-based)

This dual-source system provides both realistic gravitational pressure distribution and responsive dynamic pressure buildup.

## Core Concepts

**Hydrostatic Pressure = Weight-Based Force Distribution**
- Calculated in slices perpendicular to gravity direction
- Accumulates based on material density and gravity magnitude
- Creates realistic pressure gradients in fluid and granular materials

**Dynamic Pressure = Accumulated Transfer Resistance**
- When a cell's COM crosses a boundary but transfer is blocked
- Attempted transfer energy accumulates as pressure in target cell
- Influences future movement and transfer attempts

## Current Implementation Status

### ✅ Completed Features

**Phase 1: Dual Pressure System Foundation**
- Dual pressure fields in CellB (hydrostatic_pressure_, dynamic_pressure_, pressure_vector_)
- Basic hydrostatic calculation (currently vertical gravity only)
- Blocked transfer detection in WorldBCollisionCalculator
- Dynamic pressure accumulation with material-specific weights
- Effective density calculation

**Phase 2: Combined Pressure Forces**
- Combined pressure force calculation (hydrostatic + dynamic)
- Material-specific weighting system
- Dynamic pressure decay and dissipation
- Basic pressure force application to velocities

**Phase 4: Basic Pressure Flow**
- Pressure gradient calculation from neighbor differences
- Single-direction pressure-driven material flow
- Integration with pending moves system

### ❌ Critical Issues & Missing Features

**Architectural Issues:**
1. **Confusing Gradient System**: `getPressureGradient()` returns stored `pressure_vector_` instead of calculated gradient
2. **Single-Direction Flow**: Material can only flow to one cardinal neighbor, preventing realistic spreading
3. **Limited Neighbors**: Only checks 4 cardinal directions, missing diagonal flow
4. **No Flow Distribution**: 100% flow to one neighbor instead of proportional distribution

**Missing Features:**
- Arbitrary gravity direction support for hydrostatic calculation
- Material-specific pressure response (friction thresholds, compression)
- Multi-directional flow distribution
- Diagonal pressure gradients
- Advanced effects (waves, compression, spring-back)

## New Implementation Plan

### Phase 1: Fix Gradient System Architecture

**1.1 Separate Gradient and Force Direction** 🆕
```cpp
class CellB {
    // Existing pressure fields
    float hydrostatic_pressure_;
    float dynamic_pressure_;
    
    // NEW: Clear separation of concepts
    Vector2d dynamic_force_direction_;  // Direction of accumulated blocked forces
    // Remove pressure_vector_ to avoid confusion
    
    // Transient calculation (not stored)
    Vector2d last_pressure_gradient_;   // Cache for visualization only
    
public:
    // Clear interface
    Vector2d getDynamicForceDirection() const { return dynamic_force_direction_; }
    void setDynamicForceDirection(const Vector2d& dir) { dynamic_force_direction_ = dir; }
    
    // Gradient is calculated, not stored
    Vector2d getLastPressureGradient() const { return last_pressure_gradient_; }
};
```

**1.2 Update Pressure Calculator Interface** 🆕
```cpp
class WorldBPressureCalculator {
    // Calculate true gradient from neighbor pressures
    Vector2d calculatePressureGradient(uint32_t x, uint32_t y) const;
    
    // Get dynamic force direction from accumulated blocked transfers
    Vector2d getDynamicForceDirection(const CellB& cell) const;
    
    // NEW: Calculate gradient field for entire grid
    void updatePressureGradientField();
    
    // NEW: Cache gradient field for efficiency
    std::vector<Vector2d> gradient_field_;
};
```

### Phase 2: Implement Multi-Directional Flow 🆕

**2.1 Flow Distribution System**
```cpp
struct FlowTarget {
    Vector2i position;
    double pressure_differential;
    double flow_fraction;
    Vector2d direction;
    double distance_weight;  // 1.0 for cardinal, 0.707 for diagonal
};

std::vector<FlowTarget> calculateFlowDistribution(uint32_t x, uint32_t y) {
    std::vector<FlowTarget> targets;
    double total_weighted_differential = 0.0;
    
    const CellB& center = world_ref_.at(x, y);
    double center_pressure = center.getHydrostaticPressure() + center.getDynamicPressure();
    
    // Check all 8 neighbors
    const std::array<std::tuple<int, int, double>, 8> neighbors = {{
        {-1, 0, 1.0}, {1, 0, 1.0}, {0, -1, 1.0}, {0, 1, 1.0},  // Cardinal
        {-1, -1, 0.707}, {1, -1, 0.707}, {-1, 1, 0.707}, {1, 1, 0.707}  // Diagonal
    }};
    
    for (auto& [dx, dy, weight] : neighbors) {
        int nx = x + dx;
        int ny = y + dy;
        
        if (isValidCell(nx, ny)) {
            const CellB& neighbor = world_ref_.at(nx, ny);
            
            // Skip walls and full cells
            if (neighbor.isWall() || neighbor.getCapacity() < MIN_MATTER_THRESHOLD) {
                continue;
            }
            
            double neighbor_pressure = neighbor.getHydrostaticPressure() + neighbor.getDynamicPressure();
            double pressure_diff = center_pressure - neighbor_pressure;
            
            if (pressure_diff > MIN_FLOW_THRESHOLD) {
                // Weight by distance and material permeability
                double permeability = getMaterialPermeability(center.getMaterialType());
                double weighted_diff = pressure_diff * weight * permeability;
                
                targets.push_back({
                    Vector2i(nx, ny),
                    pressure_diff,
                    0.0,  // Will be calculated
                    Vector2d(dx, dy).normalized(),
                    weight
                });
                
                total_weighted_differential += weighted_diff;
            }
        }
    }
    
    // Calculate flow fractions
    for (auto& target : targets) {
        double weighted_diff = target.pressure_differential * target.distance_weight 
                              * getMaterialPermeability(center.getMaterialType());
        target.flow_fraction = weighted_diff / total_weighted_differential;
    }
    
    return targets;
}
```

**2.2 Apply Distributed Flow**
```cpp
std::vector<MaterialMove> calculatePressureFlow(double deltaTime) {
    std::vector<MaterialMove> pressure_moves;
    
    for (uint32_t y = 0; y < world_ref_.getHeight(); y++) {
        for (uint32_t x = 0; x < world_ref_.getWidth(); x++) {
            CellB& cell = world_ref_.at(x, y);
            
            if (cell.getFillRatio() < MIN_MATTER_THRESHOLD) continue;
            
            auto flow_targets = calculateFlowDistribution(x, y);
            if (flow_targets.empty()) continue;
            
            // Calculate total available flow
            double pressure_magnitude = std::sqrt(
                std::pow(cell.getHydrostaticPressure(), 2) + 
                std::pow(cell.getDynamicPressure(), 2)
            );
            
            double available_material = cell.getFillRatio() * PRESSURE_FLOW_RATE 
                                       * pressure_magnitude * deltaTime;
            
            // Distribute flow to all valid targets
            for (auto& target : flow_targets) {
                double flow_amount = available_material * target.flow_fraction;
                
                if (flow_amount > MIN_MATTER_THRESHOLD) {
                    MaterialMove move;
                    move.fromX = x;
                    move.fromY = y;
                    move.toX = target.position.x;
                    move.toY = target.position.y;
                    move.amount = flow_amount;
                    move.material = cell.getMaterialType();
                    move.momentum = target.direction * target.pressure_differential;
                    move.boundary_normal = target.direction;
                    move.collision_type = CollisionType::TRANSFER_ONLY;
                    
                    pressure_moves.push_back(move);
                }
            }
        }
    }
    
    return pressure_moves;
}
```

### Phase 3: Enhanced Gradient Calculation 🆕

**3.1 Proper Vector Field Gradient**
```cpp
Vector2d calculatePressureGradient(uint32_t x, uint32_t y) const {
    const CellB& center = world_ref_.at(x, y);
    double center_pressure = center.getHydrostaticPressure() + center.getDynamicPressure();
    
    Vector2d gradient(0, 0);
    double total_weight = 0.0;
    
    // Sample all 8 neighbors with distance weighting
    const std::array<std::tuple<int, int, double>, 8> neighbors = {{
        {-1, 0, 1.0}, {1, 0, 1.0}, {0, -1, 1.0}, {0, 1, 1.0},  // Cardinal
        {-1, -1, 0.707}, {1, -1, 0.707}, {-1, 1, 0.707}, {1, 1, 0.707}  // Diagonal
    }};
    
    for (auto& [dx, dy, weight] : neighbors) {
        int nx = x + dx;
        int ny = y + dy;
        
        if (isValidCell(nx, ny)) {
            const CellB& neighbor = world_ref_.at(nx, ny);
            
            // Walls create infinite pressure differential (barrier)
            if (neighbor.isWall()) {
                // Add repulsive force from wall
                gradient.x -= dx * weight * center_pressure * 0.5;
                gradient.y -= dy * weight * center_pressure * 0.5;
                total_weight += weight;
                continue;
            }
            
            double neighbor_pressure = neighbor.getHydrostaticPressure() + neighbor.getDynamicPressure();
            double pressure_diff = neighbor_pressure - center_pressure;
            
            // Accumulate weighted gradient components
            gradient.x += pressure_diff * dx * weight;
            gradient.y += pressure_diff * dy * weight;
            total_weight += weight;
        }
    }
    
    // Normalize by total weight (not neighbor count)
    if (total_weight > 0) {
        gradient = gradient / total_weight;
    }
    
    return gradient;
}
```

**3.2 Material Permeability System** 🆕
```cpp
double getMaterialPermeability(MaterialType type) const {
    switch(type) {
        case MaterialType::WATER: return 1.0;    // Full flow
        case MaterialType::SAND:  return 0.6;    // Moderate flow  
        case MaterialType::DIRT:  return 0.4;    // Restricted flow
        case MaterialType::LEAF:  return 0.3;    // Light, some flow
        case MaterialType::WOOD:  return 0.0;    // No flow (rigid)
        case MaterialType::METAL: return 0.0;    // No flow (rigid)
        case MaterialType::WALL:  return 0.0;    // No flow (barrier)
        case MaterialType::AIR:   return 1.0;    // Full flow
        default: return 0.0;
    }
}
```

### Phase 4: Arbitrary Gravity Support 🆕

**4.1 Generalized Slice-Based Hydrostatic Calculation**
```cpp
void calculateHydrostaticPressure() {
    const Vector2d gravity = world_ref_.getGravityVector();
    const double gravity_magnitude = gravity.magnitude();
    
    if (gravity_magnitude < 0.0001) {
        // Clear all hydrostatic pressure
        for (uint32_t y = 0; y < world_ref_.getHeight(); ++y) {
            for (uint32_t x = 0; x < world_ref_.getWidth(); ++x) {
                world_ref_.at(x, y).setHydrostaticPressure(0.0);
            }
        }
        return;
    }
    
    Vector2d gravity_dir = gravity.normalized();
    
    // For arbitrary gravity, we need to process cells in gravity order
    struct CellInfo {
        uint32_t x, y;
        double gravity_projection;
    };
    
    std::vector<CellInfo> all_cells;
    
    // Collect all cells with their gravity projections
    for (uint32_t y = 0; y < world_ref_.getHeight(); ++y) {
        for (uint32_t x = 0; x < world_ref_.getWidth(); ++x) {
            Vector2d cell_pos(x + 0.5, y + 0.5);  // Cell center
            double projection = cell_pos.dot(gravity_dir);
            all_cells.push_back({x, y, projection});
        }
    }
    
    // Sort cells by gravity projection (upstream to downstream)
    std::sort(all_cells.begin(), all_cells.end(), 
              [](const CellInfo& a, const CellInfo& b) {
                  return a.gravity_projection < b.gravity_projection;
              });
    
    // Process cells in gravity order
    std::map<std::pair<uint32_t, uint32_t>, double> pressure_map;
    
    for (const auto& cell_info : all_cells) {
        CellB& cell = world_ref_.at(cell_info.x, cell_info.y);
        
        // Find upstream neighbors and accumulate their pressure contributions
        double max_upstream_pressure = 0.0;
        
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) continue;
                
                int nx = cell_info.x + dx;
                int ny = cell_info.y + dy;
                
                if (isValidCell(nx, ny)) {
                    Vector2d neighbor_pos(nx + 0.5, ny + 0.5);
                    Vector2d to_current = Vector2d(cell_info.x + 0.5, cell_info.y + 0.5) - neighbor_pos;
                    
                    // Check if neighbor is upstream (against gravity)
                    if (to_current.dot(gravity_dir) > 0.0) {
                        auto it = pressure_map.find({nx, ny});
                        if (it != pressure_map.end()) {
                            double neighbor_contribution = it->second 
                                + world_ref_.at(nx, ny).getEffectiveDensity() 
                                * gravity_magnitude * to_current.magnitude();
                            max_upstream_pressure = std::max(max_upstream_pressure, neighbor_contribution);
                        }
                    }
                }
            }
        }
        
        cell.setHydrostaticPressure(max_upstream_pressure);
        pressure_map[{cell_info.x, cell_info.y}] = max_upstream_pressure;
    }
}
```

### Phase 5: Material-Specific Pressure Response 🆕

**5.1 Material Response Classification**
```cpp
enum class MaterialPressureType {
    FLUID,        // WATER - hydrostatic pressure, flows easily
    GRANULAR,     // DIRT, SAND - friction thresholds, flows under pressure
    RIGID,        // WOOD, METAL - compression only, no flow
    GASEOUS,      // AIR - compressible, flows easily
    BARRIER       // WALL - immobile pressure boundaries
};

struct MaterialPressureResponse {
    MaterialPressureType type;
    double flow_threshold;       // Minimum pressure to initiate flow
    double permeability;         // Flow rate multiplier [0,1]
    double compressibility;      // Volume reduction under pressure [0,1]
    double angle_of_repose;      // Maximum stable slope in degrees (granular only)
    double cohesive_strength;    // Pressure resistance from cohesion
};

// Material response database
const std::map<MaterialType, MaterialPressureResponse> MATERIAL_RESPONSES = {
    // Type         Type      Threshold Perm  Comp   Angle  Cohesion
    {WATER,  {FLUID,    0.0,    1.0,   0.05,  0.0,   0.0}},
    {SAND,   {GRANULAR, 0.2,    0.6,   0.10,  30.0,  0.1}},
    {DIRT,   {GRANULAR, 0.3,    0.4,   0.15,  35.0,  0.3}},
    {WOOD,   {RIGID,    999.0,  0.0,   0.02,  0.0,   5.0}},
    {METAL,  {RIGID,    999.0,  0.0,   0.01,  0.0,   10.0}},
    {LEAF,   {GRANULAR, 0.1,    0.3,   0.30,  20.0,  0.05}},
    {AIR,    {GASEOUS,  0.0,    1.0,   0.50,  0.0,   0.0}},
    {WALL,   {BARRIER,  999.0,  0.0,   0.0,   0.0,   999.0}}
};
```

**5.2 Pressure-Based Flow Control**
```cpp
bool shouldFlowUnderPressure(const CellB& cell, double total_pressure) {
    auto& response = MATERIAL_RESPONSES[cell.getMaterialType()];
    
    // Account for cohesive resistance
    double effective_threshold = response.flow_threshold + response.cohesive_strength * getCohesionFactor(cell);
    
    switch(response.type) {
        case FLUID:
        case GASEOUS:
            return true;  // Always flows
            
        case GRANULAR:
            // Check both pressure threshold and angle of repose
            if (total_pressure > effective_threshold) {
                return true;
            }
            if (exceedsAngleOfRepose(cell, response.angle_of_repose)) {
                return true;
            }
            return false;
            
        case RIGID:
        case BARRIER:
            return false;  // Never flows, only compresses
    }
}
```

**5.3 Angle of Repose Check** 🆕
```cpp
bool exceedsAngleOfRepose(const CellB& cell, double max_angle_degrees) {
    // Calculate local slope from material distribution
    Vector2d material_gradient = calculateMaterialGradient(cell.getX(), cell.getY());
    double slope_angle = std::atan2(material_gradient.y, material_gradient.x) * 180.0 / M_PI;
    
    return std::abs(slope_angle) > max_angle_degrees;
}
```

### Phase 6: Advanced Pressure Effects 🆕

**6.1 Compression Mechanics**
```cpp
void applyCompression(CellB& cell, double pressure) {
    auto& response = MATERIAL_RESPONSES[cell.getMaterialType()];
    
    if (response.compressibility > 0.0) {
        // Calculate compression based on pressure
        double compression_ratio = std::min(pressure * response.compressibility * 0.01, 0.5);
        
        // Store original fill ratio for spring-back
        cell.setUncompressedFillRatio(cell.getFillRatio());
        
        // Apply compression
        cell.setFillRatio(cell.getFillRatio() * (1.0 - compression_ratio));
        cell.setCompressionPressure(pressure);
    }
}

void releaseCompression(CellB& cell) {
    if (cell.getCompressionPressure() > 0.0) {
        // Spring-back effect
        double spring_velocity = (cell.getUncompressedFillRatio() - cell.getFillRatio()) * SPRING_CONSTANT;
        cell.addPendingForce(Vector2d(0, -spring_velocity));
        
        // Restore fill ratio
        cell.setFillRatio(cell.getUncompressedFillRatio());
        cell.setCompressionPressure(0.0);
    }
}
```

**6.2 Pressure Wave Propagation** 🆕
```cpp
class PressureWave {
    Vector2d origin;
    double initial_pressure;
    double wave_speed;
    double damping_factor;
    double current_radius;
    
public:
    void propagate(double deltaTime) {
        current_radius += wave_speed * deltaTime;
        
        // Apply pressure to cells within wave radius
        for (auto& cell : getCellsInRadius(origin, current_radius)) {
            double distance = (cell.getPosition() - origin).magnitude();
            double wave_pressure = initial_pressure * std::exp(-damping_factor * distance) 
                                  * gaussian(distance - current_radius, WAVE_WIDTH);
            
            cell.addDynamicPressure(wave_pressure);
        }
    }
};
```

## Implementation Priority Order

### Priority 1: Core Architecture Fixes
1. **Fix Gradient/Vector Confusion** (Phase 1.1-1.2)
   - Separate dynamic force direction from pressure gradient
   - Update all references to use correct methods
   - Add proper gradient calculation caching

2. **Implement Multi-Directional Flow** (Phase 2.1-2.2)
   - Add 8-neighbor flow distribution
   - Implement proportional flow splitting
   - Test with water spreading scenarios

3. **Enhanced Gradient Calculation** (Phase 3.1-3.2)
   - Add diagonal neighbor support
   - Implement distance weighting
   - Add material permeability

### Priority 2: Physical Realism
4. **Arbitrary Gravity Support** (Phase 4.1)
   - Implement generalized hydrostatic calculation
   - Test with diagonal and horizontal gravity

5. **Material-Specific Response** (Phase 5.1-5.3)
   - Add material response database
   - Implement flow thresholds
   - Add angle of repose for granular materials

### Priority 3: Advanced Features
6. **Compression Mechanics** (Phase 6.1)
   - Add compression state to cells
   - Implement spring-back forces

7. **Pressure Waves** (Phase 6.2)
   - Add wave propagation system
   - Test with impact scenarios

## Testing Scenarios

### Core Architecture Tests 🆕
- **Multi-Directional Flow**: Place high-pressure water in center, verify it flows to all 8 neighbors
- **Diagonal Gradients**: Create diagonal pressure field, verify 45° flow patterns
- **Flow Splitting**: Single source distributes proportionally to multiple sinks
- **Permeability Test**: Same pressure creates different flow rates for WATER vs SAND

### Basic Pressure Tests ✅
- **Dam Break**: Water blocked by wall should build pressure and flow when wall removed
- **Compression**: Heavy material on light material should create pressure gradient
- **Flow Channel**: Narrow passages should create pressure buildup
- **Gravity Angles**: Test hydrostatic pressure with gravity at 0°, 45°, 90°, 180°

### Material-Specific Tests 🆕
- **Granular Flow**: Sand flows only above friction threshold (0.2 pressure units)
- **Angle of Repose**: Sand maintains 30° slopes until pressure overcomes stability
- **Rigid Compression**: Wood/metal compress by 1-2% under high pressure, no flow
- **Fluid Dynamics**: Water always flows regardless of pressure threshold
- **Spring-Back**: Compressed materials expand when pressure releases

### Advanced Tests 🆕
- **Pressure Waves**: High-velocity impact creates expanding pressure wave
- **Complex Structures**: Arches and bridges show proper load distribution
- **Dynamic Loading**: Dropping weights creates transient pressure spikes
- **Mixed Materials**: Pressure transfers correctly between different material types

## Performance Considerations

### Optimization Strategies 🆕
1. **Gradient Field Caching**: Calculate full gradient field once per timestep
2. **Sparse Updates**: Only recalculate pressure for cells that changed
3. **Neighbor Lists**: Pre-compute and cache valid neighbors for each cell
4. **Pressure Regions**: Group connected high-pressure areas for batch processing
5. **LOD System**: Use simplified pressure for distant/inactive regions

### Memory Efficiency
- Store only active pressure data (cells with P > threshold)
- Use single precision floats for pressure values
- Pool pressure waves for reuse
- Implement spatial hashing for pressure regions

## Configuration Parameters

```cpp
struct PressureConfig {
    // Hydrostatic pressure settings
    double slice_thickness = 1.0;              // Cell size for pressure calculation
    double hydrostatic_multiplier = 0.002;     // Tuned for realistic water columns
    
    // Dynamic pressure settings  
    double max_dynamic_pressure = 100.0;       // Prevents numerical instability
    double dynamic_accumulation_rate = 1.0;    // Direct energy->pressure conversion
    double dynamic_decay_rate = 0.02;          // 2% decay per timestep
    double dynamic_multiplier = 1.0;           // Force scaling
    
    // Flow settings
    double pressure_flow_rate = 10.0;          // Material transfer rate
    double min_flow_threshold = 0.1;           // Minimum pressure difference for flow
    double min_matter_threshold = 0.001;       // Minimum material to consider
    
    // Gradient calculation
    double cardinal_weight = 1.0;              // Weight for N,S,E,W neighbors
    double diagonal_weight = 0.707;            // Weight for diagonal neighbors
    
    // Advanced features
    double spring_constant = 50.0;             // Compression spring-back force
    double wave_speed = 10.0;                  // Pressure wave propagation speed
    double wave_damping = 0.1;                 // Exponential wave decay
    
    // Performance
    double min_pressure_threshold = 0.01;      // Ignore pressures below this
    int gradient_update_frequency = 1;         // Update gradient every N frames
};
```

## Integration with Existing Systems

### Cohesion System
- High pressure can overcome cohesive bonds
- Pressure reduces effective cohesion strength
- Materials under compression have increased cohesion

### Collision System  
- Blocked transfers generate dynamic pressure
- High-velocity impacts create pressure waves
- Elastic collisions can reflect pressure

### Movement System
- Pressure forces integrated with gravity and adhesion
- Material flows based on combined force vectors
- Velocity limited to prevent pressure instabilities

This enhanced pressure system provides the foundation for realistic fluid dynamics, granular flow, and structural mechanics within the WorldB simulation framework.
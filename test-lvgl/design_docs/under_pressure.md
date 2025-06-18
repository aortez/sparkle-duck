# Under Pressure: Implementation Plan

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
- Attempted transfer energy accumulates as pressure within source cell
- Influences future movement and transfer attempts

## Implementation Plan

### Phase 1: Dual Pressure System Foundation

**1.1 Add Dual Pressure State to CellB** ✅ COMPLETED
```cpp
class CellB {
    float hydrostatic_pressure_;    // From gravity/weight [0, max_hydrostatic]
    float dynamic_pressure_;        // From blocked transfers [0, max_dynamic]
    Vector2d pressure_gradient_;    // Combined pressure direction
    // effective_density_ implemented as member function getEffectiveDensity() 
    // ... existing members
};
```

**Implementation Notes:**
- Added three new private fields to CellB class
- Added public accessor methods (get/set) for each pressure field
- Updated all constructors and copy operations to initialize pressure fields to 0.0
- effective_density implemented as member function rather than cached field
- Build Status: ✅ Compiles successfully

**1.2 Implement Slice-Based Hydrostatic Calculation**
```cpp
void calculateHydrostaticPressure() {
    Vector2d gravity_dir = normalize(gravity_vector);
    
    // Process slices perpendicular to gravity
    for (each slice perpendicular to gravity) {
        float accumulated_pressure = 0.0f;
        
        for (each cell in slice, following gravity direction) {
            float effective_density = cell.fill_ratio * getMaterialDensity(cell.type);
            cell.hydrostatic_pressure_ = accumulated_pressure;
            accumulated_pressure += effective_density * gravity_magnitude * slice_thickness;
        }
    }
}
```

**1.2 Detect Blocked Transfers (Dynamic Pressure)**
Modify the transfer/reflection logic to detect when transfers are blocked:
- Target cell at capacity
- Boundary collision with walls
- Material incompatibility (if implemented)

**1.4 Accumulate Dynamic Pressure Energy**
When transfers are blocked:
```cpp
// Convert blocked kinetic energy to dynamic pressure
float blocked_energy = velocity.magnitude() * transfer_amount;
dynamic_pressure_ += blocked_energy * dynamic_accumulation_rate;
pressure_gradient_ = normalize(pressure_gradient_ * getTotalPressure() + velocity * blocked_energy);
```

**1.5 Calculate Effective Density**
```cpp
float calculateEffectiveDensity(const CellB& cell) {
    return cell.fill_ratio * getMaterialDensity(cell.type);
}
```

### Phase 2: Combined Pressure Forces

**2.1 Integrated Pressure Force Calculation**
Combine hydrostatic and dynamic pressure forces:
```cpp
Vector2d calculatePressureForce(const CellB& cell) {
    // Hydrostatic component (gravity-aligned)
    Vector2d hydrostatic_force = gravity_direction * cell.hydrostatic_pressure_ * hydrostatic_multiplier;
    
    // Dynamic component (blocked-transfer direction)
    Vector2d dynamic_force = cell.pressure_gradient_ * cell.dynamic_pressure_ * dynamic_multiplier;
    
    // Material-specific weighting
    float hydrostatic_weight = getHydrostaticWeight(cell.type);
    float dynamic_weight = getDynamicWeight(cell.type);
    
    return hydrostatic_force * hydrostatic_weight + dynamic_force * dynamic_weight;
}
```

**2.2 Apply Combined Forces**
```cpp
Vector2d total_pressure_force = calculatePressureForce(cell);
velocity += total_pressure_force * timestep;
```

**2.3 Pressure Decay (Dynamic Only)**
Dynamic pressure dissipates over time, hydrostatic pressure is recalculated each frame:
```cpp
dynamic_pressure_ *= (1.0f - dynamic_decay_rate * timestep);
// Hydrostatic pressure recalculated in slice-based pass
```

**2.4 Pressure Transfer During Material Movement**
When successful transfers occur:
```cpp
void transferPressure(CellB& source, CellB& target, float transfer_ratio) {
    // Dynamic pressure transfers proportionally
    float transferred_dynamic = source.dynamic_pressure_ * transfer_ratio;
    source.dynamic_pressure_ -= transferred_dynamic;
    target.dynamic_pressure_ += transferred_dynamic * getPressureInheritance(target.type);
    
    // Hydrostatic pressure recalculated based on new material distribution
    // (handled in next hydrostatic calculation pass)
}
```

### Phase 3: Material-Specific Pressure Response

**3.1 Material Classification for Pressure Response**
```cpp
enum class MaterialPressureType {
    FLUID,        // WATER - hydrostatic pressure, flows easily
    GRANULAR,     // DIRT, SAND - friction thresholds, flows under pressure
    RIGID,        // WOOD, METAL - compression only, no flow
    WALL          // Immobile pressure boundaries
};
```

**3.2 Friction Threshold System (GridMechanics Integration)**
```cpp
void applyPressureEffects(CellB& cell) {
    float total_pressure = cell.hydrostatic_pressure_ + cell.dynamic_pressure_;
    MaterialType material = cell.type;
    
    if (isGranular(material)) { // DIRT, SAND
        if (total_pressure > getFrictionThreshold(material)) {
            enableFlow(cell, total_pressure);
        }
        applyHydrostaticFlow(cell); // Always allow some hydrostatic behavior
        
    } else if (isRigid(material)) { // WOOD, METAL
        applyCompressionOnly(cell, total_pressure);
        // No flow - only compression
        
    } else if (isFluid(material)) { // WATER
        applyHydrostaticPressure(cell, total_pressure);
        enableHighSensitivityFlow(cell);
    }
}
```

**3.3 Material-Specific Pressure Thresholds**
```cpp
struct MaterialPressureProperties {
    float friction_threshold;      // Pressure needed to overcome static friction (granular)
    float compression_threshold;   // Pressure that causes volume reduction
    float failure_threshold;       // Pressure that breaks material cohesion
    float hydrostatic_weight;      // How much hydrostatic pressure affects this material
    float dynamic_weight;          // How much dynamic pressure affects this material
    bool allows_flow;              // Whether material can flow under pressure
};
```

### Phase 4: Pressure Propagation

**4.1 Neighbor Pressure Distribution**
Pressure can spread to neighboring cells based on:
- Material compatibility
- Pressure gradients
- Cell connectivity

**4.2 Hydrostatic Pressure Effects**
For fluid-like materials, implement hydrostatic pressure:
- Pressure increases with depth in gravity direction
- Pressure equalizes across connected fluid regions

### Phase 5: Advanced Pressure Effects

**5.1 Pressure-Driven Flow**
High pressure areas push material toward lower pressure areas:
- Calculate pressure gradients between neighbors
- Apply gradient forces to COM velocity

**5.2 Compression Effects**
Very high pressure can:
- Reduce cell fill ratios (compression)
- Increase material density temporarily
- Create "spring-back" forces when pressure releases

**5.3 Pressure Waves**
Rapid pressure changes can propagate as waves:
- Useful for impact effects
- Explosion simulations
- Seismic activity

## Implementation Steps

### Step 1: Dual Pressure Foundation
1. Add dual pressure fields to CellB (hydrostatic + dynamic)
2. Implement slice-based hydrostatic pressure calculation
3. Detect blocked transfers for dynamic pressure accumulation
4. Add effective density calculation (fill_ratio * material_density)
5. Add pressure visualization to UI

### Step 2: Combined Pressure Forces
1. Implement integrated pressure force calculation
2. Apply material-specific force weighting
3. Implement dynamic pressure decay (hydrostatic recalculated)
4. Add pressure transfer during material movement
5. Test with simple blocked flow scenarios

### Step 3: Material-Specific Response (GridMechanics Integration)
1. Add material pressure classification (FLUID, GRANULAR, RIGID, WALL)
2. Implement friction threshold system for granular materials
3. Add compression-only behavior for rigid materials
4. Create material-specific pressure properties structure
5. Test different material responses (granular flow vs rigid compression)

### Step 4: Pressure Propagation
1. Implement neighbor pressure distribution
2. Add hydrostatic effects for fluids
3. Test pressure equalization

### Step 5: Advanced Effects
1. Pressure-driven flow
2. Compression mechanics
3. Pressure wave propagation

## Testing Scenarios

### Basic Pressure Tests
- **Dam Break**: Water blocked by wall should build pressure and flow when wall removed
- **Compression**: Heavy material on light material should create pressure gradient
- **Flow Channel**: Narrow passages should create pressure buildup

### Material-Specific Tests
- **Granular Flow**: Sand should flow under pressure but maintain angle of repose
- **Rigid Compression**: Wood/metal should compress but not flow under pressure
- **Fluid Dynamics**: Water should exhibit hydrostatic pressure distribution

### Advanced Tests
- **Pressure Waves**: Impact should create propagating pressure waves
- **Complex Structures**: Multi-material structures should show realistic pressure distribution
- **Dynamic Loading**: Changing loads should create realistic pressure responses

## Performance Considerations

### Optimization Strategies
1. **Lazy Pressure Calculation**: Only compute pressure for cells with recent blocked transfers
2. **Pressure Thresholding**: Ignore very low pressure values to reduce computation
3. **Batch Processing**: Update pressure in batches during dedicated pressure phase
4. **Spatial Partitioning**: Only check pressure propagation between nearby cells

### Memory Efficiency
- Use compact pressure representation (float + direction vector)
- Consider pressure pooling for cells with similar states
- Implement pressure garbage collection for inactive cells

## Future Extensions

### Pressure-Based Features
- **Hydraulic Systems**: Believable water flow.

### Integration Points
- **Cohesion/Adhesion**: Pressure affects material bonding
- **Temperature**: Pressure influences phase changes
- **Chemical Reactions**: Pressure-sensitive material transformations
- **Fracturing**: High pressure can break material bonds

## Configuration Parameters

```cpp
struct PressureConfig {
    // Hydrostatic pressure settings
    float slice_thickness = 1.0f;            // Thickness of pressure calculation slices
    float hydrostatic_multiplier = 1.0f;     // Hydrostatic force strength
    
    // Dynamic pressure settings
    float max_dynamic_pressure = 10.0f;      // Maximum dynamic pressure per cell
    float dynamic_accumulation_rate = 0.1f;  // Rate of dynamic pressure buildup
    float dynamic_decay_rate = 0.05f;        // Rate of dynamic pressure dissipation
    float dynamic_multiplier = 1.0f;         // Dynamic force strength
    
    // Material-specific properties
    std::map<MaterialType, MaterialPressureProperties> material_properties;
    
    // Performance settings
    float pressure_propagation_rate = 0.2f;  // Neighbor pressure sharing rate
    float min_pressure_threshold = 0.01f;    // Ignore pressures below this value
};

// Default material properties
material_properties[WATER] = {0.0f, 0.5f, 2.0f, 1.0f, 0.8f, true};   // High hydrostatic sensitivity
material_properties[DIRT] = {0.3f, 1.0f, 3.0f, 0.7f, 1.0f, true};    // Friction threshold required
material_properties[SAND] = {0.2f, 0.8f, 2.5f, 0.7f, 1.0f, true};    // Lower friction than dirt
material_properties[WOOD] = {999.0f, 2.0f, 5.0f, 0.3f, 0.5f, false}; // Compression only
material_properties[METAL] = {999.0f, 5.0f, 10.0f, 0.1f, 0.3f, false}; // Very rigid
```

This pressure system will add realistic material behavior and enable complex fluid and granular dynamics while maintaining the efficient cell-based simulation architecture.
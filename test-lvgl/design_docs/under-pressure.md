# Under Pressure: Slice-Based Hydrostatic Pressure System 💧

**Advanced Pressure Physics for WorldB Pure-Material Simulation**

## Overview

Implement a realistic hydrostatic pressure system in WorldB that supports arbitrary gravity directions and provides material-specific pressure responses. The system uses slice-based calculations for efficiency and physical accuracy while integrating seamlessly with the existing collision and cohesion/adhesion systems.

## Main Goals

### 1. **Realistic Fluid Dynamics** 🌊
- **Hydrostatic Pressure**: Pressure increases with depth in gravity direction
- **Pressure Equalization**: All points at same depth have equal pressure
- **Flow Behavior**: Fluids flow from high to low pressure regions
- **Material Differentiation**: Different materials respond to pressure differently

### 2. **Arbitrary Gravity Support** 🌍
- **Any Angle**: Gravity can point in any 2D direction (0° to 360°)
- **Dynamic Changes**: Gravity direction can change during simulation
- **Slice Orientation**: Pressure slices automatically orient perpendicular to gravity
- **Efficient Calculation**: O(n) pressure computation regardless of gravity angle

### 3. **Material-Specific Responses** 🧱
- **Fluid Materials**: WATER flows freely under pressure gradients
- **Granular Materials**: DIRT/SAND act fluid-like above pressure thresholds
- **Rigid Materials**: WOOD/METAL compress but don't flow
- **Hybrid Behaviors**: Materials can transition between rigid and fluid states

### 4. **Integration with Existing Systems** ⚙️
- **Force Combination**: Pressure forces combine with gravity, cohesion, adhesion
- **Collision Physics**: Pressure affects material transfer and collision behaviors
- **Performance**: Efficient slice-based calculation with per-cell storage compatibility

## Key Features

### Core Pressure Physics

#### 1. Slice-Based Calculation
```cpp
// Pressure accumulates along slices perpendicular to gravity
Vector2d gravity_direction = normalize(gravity_vector);
Vector2d slice_normal = gravity_direction;

// Each slice has uniform pressure across its width
for (each slice perpendicular to gravity) {
    slice_pressure = previous_slice_pressure + 
                    slice_average_density * gravity_magnitude * slice_thickness;
}
```

#### 2. Material Response Matrix
| Material | Pressure Response | Flow Threshold | Behavior |
|----------|-------------------|----------------|----------|
| **WATER** | High sensitivity | 0.1 | Flows immediately under pressure |
| **SAND** | Medium sensitivity | 2.0 | Granular flow above threshold |
| **DIRT** | Medium sensitivity | 1.5 | Becomes fluid-like when wet/pressurized |
| **WOOD** | Low sensitivity | 10.0 | Compresses, very high flow threshold |
| **METAL** | Very low sensitivity | 50.0 | Minimal compression, extremely high flow threshold |
| **LEAF** | High sensitivity | 0.5 | Light material, easily displaced |
| **WALL** | No response | ∞ | Immobile, provides structural support |
| **AIR** | No response | 0.0 | No resistance to flow |

#### 3. Pressure-Driven Forces
```cpp
struct PressureForce {
    Vector2d direction;        // Direction of pressure gradient
    double magnitude;          // Strength of pressure differential
    double flow_rate;          // Material-specific flow response
    bool above_threshold;      // Whether material should flow
};
```

### Advanced Features

#### 1. Pressure Gradient Flows
- **Gradient Detection**: Calculate pressure differences between adjacent cells
- **Flow Direction**: Materials flow from high to low pressure
- **Flow Rate**: Proportional to pressure gradient and material properties
- **Equilibrium**: System reaches stable state when pressure gradients balance

#### 2. Material State Transitions
- **Pressure Thresholds**: Materials change behavior under high pressure
- **Granular → Fluid**: SAND/DIRT become fluid-like above pressure limits
- **Compression Effects**: Rigid materials compress under extreme pressure
- **Structural Failure**: Materials may fragment under excessive pressure

#### 3. Container Physics
- **Pressure Vessels**: WALL materials contain pressure
- **Pressure Relief**: Materials flow around obstacles
- **Dam Mechanics**: Pressure buildup behind barriers
- **Structural Loads**: Pressure forces on container walls

## Implementation Plan

### Phase 1: Core Slice Infrastructure 🏗️

#### 1.1 Slice Calculation System
```cpp
// New classes in WorldB
class PressureSlice {
public:
    Vector2d slice_normal;              // Direction perpendicular to gravity
    std::vector<Vector2i> cell_positions; // Cells belonging to this slice
    double slice_pressure;              // Calculated pressure for this slice
    double average_density;             // Average material density in slice
    double slice_thickness;             // Physical thickness of slice
    
    void calculatePressure(double gravity_magnitude, double previous_pressure);
    void distributePressureToCells(WorldB& world);
};

class PressureSystem {
public:
    std::vector<PressureSlice> slices;
    Vector2d gravity_direction;
    
    void rebuildSlices(const WorldB& world);
    void calculateAllPressures(double gravity_magnitude);
    void applyPressuresToWorld(WorldB& world);
};
```

#### 1.2 Slice Generation Algorithm
```cpp
// Generate slices perpendicular to gravity vector
void PressureSystem::rebuildSlices(const WorldB& world) {
    slices.clear();
    
    // For each possible slice position along gravity direction
    Vector2d perpendicular = Vector2d(-gravity_direction.y, gravity_direction.x);
    
    // Project all cells onto gravity axis to determine slice membership
    for (uint32_t x = 0; x < world.getWidth(); ++x) {
        for (uint32_t y = 0; y < world.getHeight(); ++y) {
            Vector2d cell_pos(x, y);
            double projection = dot(cell_pos, gravity_direction);
            
            // Find or create slice for this projection value
            PressureSlice& slice = getSliceForProjection(projection);
            slice.cell_positions.push_back(Vector2i(x, y));
        }
    }
    
    // Sort slices by projection (from low to high pressure)
    std::sort(slices.begin(), slices.end(), [](const auto& a, const auto& b) {
        return a.slice_projection < b.slice_projection;
    });
}
```

#### 1.3 Integration Points
- **Target File**: `src/WorldB.cpp`
- **Method**: `WorldB::calculateHydrostaticPressure()`
- **Call Site**: `WorldB::advanceTime()` → `applyPressure(deltaTime)`

### Phase 2: Pressure Force Integration ⚡

#### 2.1 Enhanced Movement System
```cpp
// Modify WorldB::queueMaterialMoves() to include pressure forces
void WorldB::queueMaterialMoves(double deltaTime) {
    // Existing forces
    for (each cell) {
        Vector2d gravity_force = calculateGravityForce(cell);
        CohesionForce cohesion = calculateCohesionForce(x, y);
        AdhesionForce adhesion = calculateAdhesionForce(x, y);
        
        // NEW: Add pressure forces
        PressureForce pressure = calculatePressureForce(x, y);
        
        // Combine all forces
        Vector2d net_driving_force = gravity_force + 
                                   adhesion.force_direction + 
                                   pressure.direction * pressure.magnitude;
        
        double total_resistance = cohesion.resistance_magnitude;
        
        // Movement decision with pressure consideration
        if (net_driving_force.magnitude() > total_resistance && 
            pressure.above_threshold) {
            queueMaterialMove(x, y, net_driving_force, deltaTime);
        }
    }
}
```

#### 2.2 Pressure Force Calculation
```cpp
PressureForce WorldB::calculatePressureForce(uint32_t x, uint32_t y) {
    const CellB& cell = at(x, y);
    if (cell.isEmpty()) return {Vector2d(0,0), 0.0, 0.0, false};
    
    // Get material pressure response properties
    const auto& props = cell.getMaterialProperties();
    double flow_threshold = props.pressure_flow_threshold;
    double pressure_sensitivity = props.pressure_sensitivity;
    
    // Calculate pressure gradient in neighborhood
    Vector2d pressure_gradient = calculatePressureGradient(x, y);
    
    // Determine if above flow threshold
    double current_pressure = cell.getPressure();
    bool above_threshold = current_pressure > flow_threshold;
    
    // Calculate flow rate based on material properties
    double flow_rate = above_threshold ? 
        pressure_sensitivity * (current_pressure - flow_threshold) : 0.0;
    
    return {
        pressure_gradient.normalized(),
        pressure_gradient.magnitude() * flow_rate,
        flow_rate,
        above_threshold
    };
}
```

### Phase 3: Material-Specific Behaviors 🎯

#### 3.1 Enhanced Material Properties
```cpp
// Add to MaterialProperties struct
struct MaterialProperties {
    // Existing properties...
    double density;
    double elasticity;
    double cohesion;
    double adhesion;
    
    // NEW: Pressure-specific properties
    double pressure_sensitivity;      // How strongly material responds to pressure
    double pressure_flow_threshold;   // Minimum pressure for flow behavior
    double compression_factor;        // How much material compresses under pressure
    bool is_compressible;            // Whether material volume changes under pressure
    bool supports_shear_stress;      // Can material support sideways forces (granular materials)
};
```

#### 3.2 Material-Specific Pressure Responses
```cpp
// Material configuration
const MaterialProperties MATERIAL_PROPS[] = {
    // WATER: High sensitivity, immediate flow
    {MaterialType::WATER, density: 1.0, pressure_sensitivity: 1.0, 
     pressure_flow_threshold: 0.1, is_compressible: false, supports_shear_stress: false},
    
    // SAND: Medium sensitivity, granular behavior
    {MaterialType::SAND, density: 1.6, pressure_sensitivity: 0.7, 
     pressure_flow_threshold: 2.0, is_compressible: true, supports_shear_stress: true},
    
    // DIRT: Medium sensitivity, becomes fluid when pressurized
    {MaterialType::DIRT, density: 1.3, pressure_sensitivity: 0.6, 
     pressure_flow_threshold: 1.5, is_compressible: true, supports_shear_stress: true},
    
    // METAL: Very low sensitivity, high compression resistance
    {MaterialType::METAL, density: 7.8, pressure_sensitivity: 0.1, 
     pressure_flow_threshold: 50.0, is_compressible: false, supports_shear_stress: true}
};
```

### Phase 4: Advanced Pressure Effects 🌪️

#### 4.1 Pressure Gradient Visualization
```cpp
// Debug rendering for pressure visualization
void WorldB::drawPressureDebug(lv_obj_t* canvas) {
    for (uint32_t x = 0; x < width_; ++x) {
        for (uint32_t y = 0; y < height_; ++y) {
            const CellB& cell = at(x, y);
            double pressure = cell.getPressure();
            
            // Color intensity based on pressure
            lv_color_t pressure_color = lv_color_mix(
                lv_color_make(255, 0, 0),  // Red for high pressure
                lv_color_make(0, 0, 255),  // Blue for low pressure
                static_cast<uint8_t>(pressure * 255 / max_pressure)
            );
            
            // Draw pressure gradient arrows
            Vector2d gradient = calculatePressureGradient(x, y);
            if (gradient.magnitude() > 0.1) {
                drawArrow(canvas, x, y, gradient, pressure_color);
            }
        }
    }
}
```

#### 4.2 Container and Structural Effects
```cpp
// Pressure effects on structural materials
void WorldB::applyStructuralPressure(double deltaTime) {
    for (uint32_t x = 0; x < width_; ++x) {
        for (uint32_t y = 0; y < height_; ++y) {
            CellB& cell = at(x, y);
            
            if (cell.getMaterialType() == MaterialType::WALL) {
                // Calculate pressure load on wall from adjacent cells
                double pressure_load = calculateWallPressureLoad(x, y);
                
                // Wall failure under extreme pressure (future feature)
                if (pressure_load > WALL_FAILURE_THRESHOLD) {
                    // TODO: Implement wall structural failure
                }
            }
        }
    }
}
```

### Phase 5: Performance Optimization & Polish 🚀

#### 5.1 Efficient Slice Updates
```cpp
// Only recalculate slices when gravity direction changes
class PressureSystem {
private:
    Vector2d last_gravity_direction;
    bool slices_need_rebuild;
    
public:
    void updatePressures(const WorldB& world, Vector2d current_gravity) {
        // Check if gravity direction changed significantly
        if (distance(current_gravity.normalized(), last_gravity_direction) > 0.01) {
            rebuildSlices(world);
            last_gravity_direction = current_gravity.normalized();
        }
        
        // Always recalculate pressure values (material distribution changes)
        calculateAllPressures(current_gravity.magnitude());
        applyPressuresToWorld(const_cast<WorldB&>(world));
    }
};
```

#### 5.2 Spatial Optimization
```cpp
// Skip pressure calculations for regions with no material
void PressureSystem::calculateAllPressures(double gravity_magnitude) {
    double accumulated_pressure = 0.0;
    
    for (PressureSlice& slice : slices) {
        // Skip empty slices
        if (slice.average_density < MIN_DENSITY_THRESHOLD) {
            slice.slice_pressure = accumulated_pressure;
            continue;
        }
        
        // Calculate pressure for non-empty slices
        accumulated_pressure += slice.average_density * gravity_magnitude * slice.slice_thickness;
        slice.slice_pressure = accumulated_pressure;
    }
}
```

## Implementation Timeline

### **Phase 1: Foundation (Week 1)**
- ✅ Slice calculation infrastructure
- ✅ Basic pressure distribution to cells
- ✅ Integration with existing physics pipeline

### **Phase 2: Force Integration (Week 2)**  
- ✅ Pressure force calculation
- ✅ Integration with cohesion/adhesion system
- ✅ Basic material flow behaviors

### **Phase 3: Material Behaviors (Week 3)**
- ✅ Material-specific pressure responses
- ✅ Pressure threshold systems
- ✅ Granular vs fluid material transitions

### **Phase 4: Advanced Features (Week 4)**
- ✅ Pressure visualization and debugging
- ✅ Container physics and structural effects
- ✅ Performance optimization

### **Phase 5: Testing & Polish (Week 5)**
- ✅ Comprehensive unit tests
- ✅ Visual validation scenarios
- ✅ Performance profiling and optimization

## Success Metrics

### **Fluid Dynamics** 💧
- ✅ **Water flows naturally** under pressure gradients
- ✅ **Pressure equalizes** horizontally at same depth
- ✅ **Dam physics** - water pressure builds behind barriers
- ✅ **Container effects** - pressure contained by walls

### **Material Differentiation** 🎯
- ✅ **Granular flow** - sand/dirt behave differently under pressure
- ✅ **Rigid resistance** - wood/metal resist pressure-driven flow
- ✅ **Threshold behaviors** - materials transition between states
- ✅ **Pressure sensitivity** - materials respond according to their properties

### **Arbitrary Gravity** 🌍
- ✅ **Any angle** - pressure system works with gravity in any direction
- ✅ **Dynamic changes** - pressure adjusts when gravity changes
- ✅ **Efficiency** - performance remains good regardless of gravity angle

### **System Integration** ⚙️
- ✅ **Force combination** - pressure works alongside cohesion/adhesion
- ✅ **Collision compatibility** - pressure affects material transfers
- ✅ **Performance** - maintains real-time simulation speeds

## Technical Architecture

### **Core Classes**
- **PressureSlice**: Individual slice with cells and pressure calculation
- **PressureSystem**: Manages all slices and coordinates pressure updates
- **PressureForce**: Encapsulates pressure-driven forces on materials
- **MaterialProperties**: Extended with pressure-specific parameters

### **Integration Points**
- **WorldB::calculateHydrostaticPressure()**: Main pressure calculation entry point
- **WorldB::queueMaterialMoves()**: Pressure forces added to movement decisions
- **CellB::getPressure()/setPressure()**: Per-cell pressure storage
- **MaterialProperties**: Pressure sensitivity and flow thresholds

### **Performance Considerations**
- **Slice Caching**: Only rebuild slices when gravity direction changes
- **Sparse Calculation**: Skip pressure calculations for empty regions
- **Efficient Data Structures**: Minimize memory allocations during updates
- **Parallelization Ready**: Slice calculations are independent and parallelizable

## Testing Strategy

### **Unit Test Framework**

Building on the existing GoogleTest infrastructure with `PressureSystem_test.cpp` and `WorldBVisual_test.cpp`, comprehensive unit tests will validate each component:

#### **Core Infrastructure Tests**
```cpp
class PressureSliceTest : public ::testing::Test {
protected:
    // Test slice generation and cell membership
    TEST_F(PressureSliceTest, SliceGeneration_VerticalGravity);
    TEST_F(PressureSliceTest, SliceGeneration_HorizontalGravity);  
    TEST_F(PressureSliceTest, SliceGeneration_DiagonalGravity);
    TEST_F(PressureSliceTest, SliceGeneration_ArbitraryAngle);
    
    // Test pressure accumulation math
    TEST_F(PressureSliceTest, PressureAccumulation_UniformDensity);
    TEST_F(PressureSliceTest, PressureAccumulation_VariedDensity);
    TEST_F(PressureSliceTest, PressureAccumulation_PartialFills);
    TEST_F(PressureSliceTest, PressureAccumulation_EmptySlices);
    
    // Test slice sorting and ordering
    TEST_F(PressureSliceTest, SliceSorting_ByProjection);
    TEST_F(PressureSliceTest, SliceSorting_WithNegativeProjections);
};
```

#### **Material Response Tests**
```cpp
class MaterialPressureTest : public ::testing::Test {
protected:
    // Test material-specific thresholds
    TEST_F(MaterialPressureTest, FlowThresholds_AllMaterials);
    TEST_F(MaterialPressureTest, PressureSensitivity_WATER);
    TEST_F(MaterialPressureTest, PressureSensitivity_SAND_Granular);
    TEST_F(MaterialPressureTest, PressureSensitivity_METAL_Rigid);
    
    // Test state transitions
    TEST_F(MaterialPressureTest, GranularToFluid_SAND_AboveThreshold);
    TEST_F(MaterialPressureTest, GranularToFluid_DIRT_PressurizedFlow);
    TEST_F(MaterialPressureTest, RigidCompression_WOOD_METAL);
    
    // Test material interaction matrix
    TEST_F(MaterialPressureTest, MaterialInteraction_FluidOnRigid);
    TEST_F(MaterialPressureTest, MaterialInteraction_GranularStacking);
};
```

#### **Force Integration Tests**
```cpp
class PressureForceTest : public ::testing::Test {
protected:
    // Test gradient calculations
    TEST_F(PressureForceTest, PressureGradient_SimpleColumn);
    TEST_F(PressureForceTest, PressureGradient_ComplexDistribution);
    TEST_F(PressureForceTest, PressureGradient_BoundaryEffects);
    
    // Test force combination with existing systems
    TEST_F(PressureForceTest, ForceCombination_WithGravity);
    TEST_F(PressureForceTest, ForceCombination_WithCohesion);
    TEST_F(PressureForceTest, ForceCombination_WithAdhesion);
    TEST_F(PressureForceTest, ForceCombination_NetMovementDecision);
    
    // Test flow rate calculations
    TEST_F(PressureForceTest, FlowRate_PressureDriven);
    TEST_F(PressureForceTest, FlowRate_MaterialSpecific);
};
```

#### **System Integration Tests**
```cpp
class PressureSystemIntegrationTest : public ::testing::Test {
protected:
    // Test complete system behavior
    TEST_F(PressureSystemIntegrationTest, FullUpdate_VerticalGravity);
    TEST_F(PressureSystemIntegrationTest, FullUpdate_ArbitraryGravity);
    TEST_F(PressureSystemIntegrationTest, GravityChange_SliceRebuild);
    TEST_F(PressureSystemIntegrationTest, DynamicGravity_Performance);
    
    // Test edge cases
    TEST_F(PressureSystemIntegrationTest, EmptyWorld_NoPressure);
    TEST_F(PressureSystemIntegrationTest, SingleCell_NoPressure);
    TEST_F(PressureSystemIntegrationTest, UniformMaterial_LinearPressure);
};
```

### **Visual Test Scenarios**

Leveraging the existing `WorldBVisualTest` framework for physics validation:

#### **Fluid Dynamics Validation**
```cpp
class PressureVisualTest : public WorldBVisualTest {
protected:
    // Dam physics and pressure buildup
    TEST_F(PressureVisualTest, DamPhysics_WaterBehindWall);
    TEST_F(PressureVisualTest, DamBreak_PressureRelease);
    TEST_F(PressureVisualTest, Siphon_PressureDriven);
    
    // Hydrostatic pressure verification
    TEST_F(PressureVisualTest, HydrostaticEquilibrium_SameDepth);
    TEST_F(PressureVisualTest, PressureGradient_VerticalColumn);
    TEST_F(PressureVisualTest, PressureDistribution_ComplexShape);
};
```

#### **Material Behavior Scenarios**
```cpp
class MaterialBehaviorVisualTest : public WorldBVisualTest {
protected:
    // Granular material behaviors
    TEST_F(MaterialBehaviorVisualTest, SandFlow_AngleOfRepose);
    TEST_F(MaterialBehaviorVisualTest, SandPile_GranularStability);
    TEST_F(MaterialBehaviorVisualTest, DirtFlow_PressureThreshold);
    
    // Rigid material responses
    TEST_F(MaterialBehaviorVisualTest, MetalStack_CompressionOnly);
    TEST_F(MaterialBehaviorVisualTest, WoodBeam_StructuralLoad);
    
    // Mixed material scenarios
    TEST_F(MaterialBehaviorVisualTest, LayeredMaterials_DifferentialPressure);
    TEST_F(MaterialBehaviorVisualTest, MaterialSeparation_DensityBased);
};
```

#### **Arbitrary Gravity Tests**
```cpp
class ArbitraryGravityVisualTest : public WorldBVisualTest {
protected:
    // Different gravity angles
    TEST_F(ArbitraryGravityVisualTest, RightGravity_HorizontalPressure);
    TEST_F(ArbitraryGravityVisualTest, DiagonalGravity_SlantedPressure);
    TEST_F(ArbitraryGravityVisualTest, UpsideDownGravity_InvertedPressure);
    
    // Dynamic gravity changes
    TEST_F(ArbitraryGravityVisualTest, GravityRotation_PressureReorientation);
    TEST_F(ArbitraryGravityVisualTest, GravityFlip_PressureReversal);
};
```

### **Performance Test Suite**

#### **Benchmarking Framework**
```cpp
class PressureBenchmarkTest : public ::testing::Test {
protected:
    // Slice rebuilding performance
    TEST_F(PressureBenchmarkTest, SliceRebuild_LargeWorld_VerticalGravity);
    TEST_F(PressureBenchmarkTest, SliceRebuild_LargeWorld_ArbitraryGravity);
    TEST_F(PressureBenchmarkTest, SliceRebuild_FrequencyOptimization);
    
    // Pressure calculation scaling
    TEST_F(PressureBenchmarkTest, PressureCalculation_WorldSize_Scaling);
    TEST_F(PressureBenchmarkTest, PressureCalculation_MaterialDensity_Impact);
    TEST_F(PressureBenchmarkTest, SparseCalculation_EmptyRegions);
    
    // Memory allocation profiling
    TEST_F(PressureBenchmarkTest, MemoryAllocations_SliceStorage);
    TEST_F(PressureBenchmarkTest, MemoryAllocations_PressureDistribution);
};
```

### **Testing Validation Metrics**

#### **Mathematical Correctness**
- ✅ **Hydrostatic Formula**: `pressure[slice] = pressure[previous] + density * gravity * thickness`
- ✅ **Pressure Equilibrium**: Same depth → same pressure (within tolerance)
- ✅ **Force Conservation**: Net forces balance in equilibrium states
- ✅ **Energy Conservation**: No spurious pressure energy creation

#### **Physical Realism**
- ✅ **Material Responses**: Each material behaves according to its properties
- ✅ **Flow Directions**: Materials flow from high to low pressure
- ✅ **Threshold Behaviors**: Granular → fluid transitions occur correctly
- ✅ **Container Physics**: Walls properly contain pressure

#### **Performance Requirements**
- ✅ **Real-time Performance**: Maintains >30 FPS with 50×50 worlds
- ✅ **Scaling**: O(n) complexity confirmed through benchmarks
- ✅ **Memory Efficiency**: Minimal allocations during pressure updates
- ✅ **Cache Efficiency**: Slice rebuilding only when necessary

#### **Integration Compatibility**
- ✅ **Force Combination**: Works seamlessly with cohesion/adhesion
- ✅ **Collision System**: Pressure affects material transfer behaviors
- ✅ **UI Integration**: Debug visualization functions correctly
- ✅ **Material System**: All 8 material types respond appropriately

### **Test Execution Strategy**

#### **Development Phase Testing**
1. **Phase 1**: Unit tests for slice infrastructure (`PressureSliceTest`)
2. **Phase 2**: Force integration tests (`PressureForceTest`)  
3. **Phase 3**: Material behavior validation (`MaterialPressureTest`)
4. **Phase 4**: Visual scenario testing (`PressureVisualTest`)
5. **Phase 5**: Performance benchmarking (`PressureBenchmarkTest`)

#### **Continuous Integration**
- **Fast Unit Tests**: Run on every commit (< 30 seconds)
- **Integration Tests**: Run on pull requests (< 5 minutes)
- **Visual Tests**: Run nightly with `SPARKLE_DUCK_VISUAL_TESTS=1`
- **Performance Tests**: Run weekly for regression detection

#### **Manual Validation**
- **Interactive Testing**: Manual scenarios with debug visualization
- **Physics Demonstrations**: Dam break, sandpile, material separation
- **Edge Case Exploration**: Extreme gravity angles, mixed materials
- **User Experience**: Intuitive pressure behaviors during interaction

---

**Status**: Ready for implementation  
**Dependencies**: Cohesion/adhesion system (in progress)  
**Risk Level**: Medium (complex algorithm, performance-sensitive)  
**Estimated Implementation Time**: 3-5 weeks
**Testing Confidence**: High (comprehensive unit and visual test coverage)
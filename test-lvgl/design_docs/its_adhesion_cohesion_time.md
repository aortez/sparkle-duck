# It's Adhesion & Cohesion Time! 🧲

**Implementation Plan for Contact-Based Material Forces in WorldB**

## Overview

Add realistic cohesion and adhesion forces to WorldB that differentiate material behaviors without creating unrealistic "magnetism" between distant particles. Forces only act between adjacent cells (contact-only physics).

## Core Design Principles

### 1. Contact-Only Forces
- **No Action at Distance**: Cohesion/adhesion only affects adjacent cells
- **Realistic Physics**: Materials interact only when physically touching
- **No Migration**: Separated particles behave independently until contact

### 2. Force Types
- **Cohesion**: Resistance to breaking apart from same-material neighbors
- **Adhesion**: Attraction/friction between different adjacent materials  
- **Movement Threshold**: Forces determine minimum energy needed for movement

## Implementation Phases

### Phase 1: Core Force Calculation System

#### 1.1 Add Force Calculation Methods to WorldB
```cpp
// New methods in WorldB.h/cpp
struct CohesionForce {
    double resistance_magnitude;  // Strength of cohesive resistance
    uint32_t connected_neighbors; // Number of same-material neighbors
};

struct AdhesionForce {
    Vector2d force_direction;     // Direction of adhesive pull/resistance
    double force_magnitude;       // Strength of adhesive force
    MaterialType target_material; // Strongest interacting material
    uint32_t contact_points;      // Number of contact interfaces
};

CohesionForce calculateCohesionForce(uint32_t x, uint32_t y);
AdhesionForce calculateAdhesionForce(uint32_t x, uint32_t y);
```

#### 1.2 Integration Points
- **Target File**: `src/WorldB.cpp:queueMaterialMoves()`
- **Integration**: Add force calculations before movement decisions
- **Threshold Logic**: Movement only when driving force > resistance

### Phase 2: Movement Logic Integration 

#### 2.1 Enhanced Movement Decision System
```cpp
void WorldB::queueMaterialMoves(double deltaTime) {
    for (each cell) {
        // Current forces
        Vector2d gravity_force = calculateGravityForce(cell);
        
        // NEW: Add cohesion/adhesion
        CohesionForce cohesion = calculateCohesionForce(x, y);
        AdhesionForce adhesion = calculateAdhesionForce(x, y);
        
        // Net driving force
        Vector2d driving_force = gravity_force + adhesion.force_direction;
        
        // Movement threshold from cohesion
        double movement_threshold = cohesion.resistance_magnitude;
        
        // Move only if force overcomes resistance
        if (driving_force.magnitude() > movement_threshold) {
            queueMaterialMove(x, y, driving_force.normalized(), deltaTime);
        }
    }
}
```

#### 2.2 Force-Based Material Behaviors
- **WATER (cohesion=0.1, adhesion=0.1)**: Flows freely, minimal resistance
- **DIRT (cohesion=0.4, adhesion=0.2)**: Some clumping, granular flow  
- **METAL (cohesion=0.9, adhesion=0.4)**: Strong resistance to separation
- **WOOD (cohesion=0.7, adhesion=0.3)**: Interlocking behavior

### Phase 3: Material-Specific Interactions 

#### 3.1 Adhesion Interaction Matrix
```cpp
// Mutual adhesion calculations
double calculateMutualAdhesion(MaterialType mat1, MaterialType mat2) {
    const auto& props1 = getMaterialProperties(mat1);
    const auto& props2 = getMaterialProperties(mat2);
    
    // Geometric mean for mutual attraction
    return std::sqrt(props1.adhesion * props2.adhesion);
}
```

#### 3.2 Special Material Combinations
- **WATER + DIRT**: Moderate mutual adhesion → mud-like behavior
- **WATER + WALL**: High adhesion → water sticks to surfaces
- **METAL + WOOD**: Medium adhesion → friction resistance
- **LEAF + AIR**: Very low adhesion → easily moves

### Phase 4: Testing & Validation 

#### 4.1 Unit Tests
```cpp
// New test files
- tests/CohesionForces_test.cpp
- tests/AdhesionForces_test.cpp  
- tests/MaterialBehavior_test.cpp
```

#### 4.2 Validation Scenarios
- **Water Pooling**: Low cohesion allows natural pooling behavior
- **Sand Piles**: Medium cohesion creates realistic pile formation
- **Metal Clumping**: High cohesion keeps touching metal together
- **Wood Structures**: High cohesion maintains beam integrity

### Phase 5: Performance & Polish 

#### 5.1 Performance Optimization
- **Neighbor Caching**: Cache neighbor lookups for force calculations
- **Force Thresholding**: Skip force calculations for materials below threshold
- **Selective Updates**: Only recalculate forces when neighbors change

#### 5.2 Debug & Visualization
- **Force Visualization**: Optional arrows showing cohesion/adhesion forces
- **Debug Logging**: Trace force calculations for material behavior analysis
- **Parameter Tuning**: Expose cohesion/adhesion multipliers for experimentation

## Detailed Force Mechanics

### Force Calculation Specifics

#### 1. Cohesion Force Calculation
```cpp
CohesionForce calculateCohesionForce(uint32_t x, uint32_t y) {
    const CellB& cell = at(x, y);
    if (cell.isEmpty()) return {0.0, 0};
    
    double material_cohesion = getMaterialProperties(cell.getMaterialType()).cohesion;
    uint32_t connected_neighbors = 0;
    
    // Check all 8 neighbors (including diagonals)
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0) continue; // Skip self
            
            uint32_t nx = x + dx, ny = y + dy;
            if (isValidCell(nx, ny)) {
                const CellB& neighbor = at(nx, ny);
                if (neighbor.getMaterialType() == cell.getMaterialType() && 
                    neighbor.getFillRatio() > MIN_FILL_THRESHOLD) {
                    
                    // Weight by neighbor's fill ratio for partial cells
                    connected_neighbors += neighbor.getFillRatio();
                }
            }
        }
    }
    
    // Resistance magnitude = cohesion × connection strength × own fill ratio
    double resistance = material_cohesion * connected_neighbors * cell.getFillRatio();
    return {resistance, static_cast<uint32_t>(connected_neighbors)};
}
```

#### 2. Adhesion Force Calculation  
```cpp
AdhesionForce calculateAdhesionForce(uint32_t x, uint32_t y) {
    const CellB& cell = at(x, y);
    if (cell.isEmpty()) return {{0.0, 0.0}, 0.0, MaterialType::AIR, 0};
    
    const auto& props = getMaterialProperties(cell.getMaterialType());
    Vector2d total_force(0.0, 0.0);
    uint32_t contact_count = 0;
    MaterialType strongest_attractor = MaterialType::AIR;
    double max_adhesion = 0.0;
    
    // Check all 8 neighbors for different materials
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0) continue;
            
            uint32_t nx = x + dx, ny = y + dy;
            if (isValidCell(nx, ny)) {
                const CellB& neighbor = at(nx, ny);
                
                if (neighbor.getMaterialType() != cell.getMaterialType() && 
                    neighbor.getFillRatio() > MIN_FILL_THRESHOLD) {
                    
                    // Calculate mutual adhesion (geometric mean)
                    const auto& neighbor_props = getMaterialProperties(neighbor.getMaterialType());
                    double mutual_adhesion = std::sqrt(props.adhesion * neighbor_props.adhesion);
                    
                    // Direction vector toward neighbor (normalized)
                    Vector2d direction(dx, dy);
                    direction.normalize();
                    
                    // Force strength weighted by fill ratios and distance
                    double distance_weight = (std::abs(dx) + std::abs(dy) == 1) ? 1.0 : 0.707; // Adjacent vs diagonal
                    double force_strength = mutual_adhesion * neighbor.getFillRatio() * 
                                          cell.getFillRatio() * distance_weight;
                    
                    total_force += direction * force_strength;
                    contact_count++;
                    
                    if (mutual_adhesion > max_adhesion) {
                        max_adhesion = mutual_adhesion;
                        strongest_attractor = neighbor.getMaterialType();
                    }
                }
            }
        }
    }
    
    return {total_force, total_force.magnitude(), strongest_attractor, contact_count};
}
```

### World Boundary Handling

#### 1. Force Calculation at Boundaries
- **Invalid Neighbors**: Treated as AIR (no cohesion, minimal adhesion)
- **WALL Boundaries**: Special case for adhesion calculation
- **Edge Effects**: Reduced neighbor count naturally decreases cohesion resistance

```cpp
// In force calculation loops:
if (!isValidCell(nx, ny)) {
    if (walls_enabled_) {
        // Treat boundary as WALL material for adhesion
        // No cohesion (different material type)
        // Add WALL adhesion force toward boundary
    } else {
        // Treat as AIR - no forces
        continue;
    }
}
```

#### 2. Boundary Movement Behavior
- **Reduced Cohesion**: Edge materials have fewer neighbors → lower resistance
- **Wall Adhesion**: Materials stick to WALL boundaries based on adhesion values
- **Natural Spreading**: Low-cohesion materials (WATER) spread along boundaries

### Partial Cell Handling

#### 1. Fill Ratio Weighting
- **Force Scaling**: All forces proportional to `cell.getFillRatio()`
- **Neighbor Contribution**: Neighbor forces weighted by `neighbor.getFillRatio()`  
- **Threshold Filtering**: Cells below `MIN_FILL_THRESHOLD` ignored

#### 2. Partial Transfer Effects
```cpp
// During movement decision:
if (driving_force.magnitude() > cohesion_resistance) {
    // Transfer amount limited by available material
    double transfer_amount = std::min(cell.getFillRatio(), 
                                    1.0 - target_cell.getFillRatio());
    
    // Remaining material retains proportional cohesion
    double remaining_ratio = cell.getFillRatio() - transfer_amount;
    if (remaining_ratio > MIN_FILL_THRESHOLD) {
        // Recalculate forces for remaining material
    }
}
```

### WALL Material Interactions

#### 1. Cohesion with WALL
- **No Cohesion**: WALL ≠ other materials → no cohesive resistance
- **Immobile**: WALL never moves regardless of forces
- **Structural Support**: WALL provides rigid framework for other materials

#### 2. Adhesion with WALL
- **High Adhesion**: WALL has adhesion=1.0 → strong binding
- **Material Sticking**: WATER, DIRT stick to walls based on mutual adhesion
- **Friction Effects**: Movement along walls requires overcoming adhesive resistance

## Integration with Existing Collision System

### Modified MaterialMove Structure
```cpp
struct MaterialMove {
    // Existing fields...
    uint32_t fromX, fromY, toX, toY;
    double amount;
    MaterialType material;
    Vector2d momentum;
    Vector2d boundary_normal;
    CollisionType collision_type;
    
    // NEW: Force-related data
    double cohesion_resistance;        // Resistance to movement from cohesion
    double adhesion_magnitude;         // Strength of adhesive binding
    Vector2d adhesion_direction;       // Direction of adhesive pull
    bool force_override;               // True if movement forced despite resistance
};
```

### Enhanced createCollisionAwareMove() Integration
```cpp
// At WorldB.cpp:652-654, modify createCollisionAwareMove():
MaterialMove WorldB::createCollisionAwareMove(const CellB& fromCell, const CellB& toCell, 
                                              const Vector2i& fromPos, const Vector2i& toPos,
                                              const Vector2i& direction, double deltaTime) {
    MaterialMove move;
    
    // Existing collision detection logic...
    move.collision_type = determineCollisionType(fromCell.getMaterialType(), 
                                               toCell.getMaterialType(), 
                                               move.collision_energy);
    
    // NEW: Add force data to collision calculation
    CohesionForce cohesion = calculateCohesionForce(fromPos.x, fromPos.y);
    AdhesionForce adhesion = calculateAdhesionForce(fromPos.x, fromPos.y);
    
    move.cohesion_resistance = cohesion.resistance_magnitude;
    move.adhesion_magnitude = adhesion.force_magnitude;
    move.adhesion_direction = adhesion.force_direction;
    
    // Modify collision behavior based on binding forces
    if (move.cohesion_resistance > STRONG_COHESION_THRESHOLD) {
        // High cohesion prevents fragmentation
        if (move.collision_type == CollisionType::FRAGMENTATION) {
            move.collision_type = CollisionType::ELASTIC_REFLECTION;
        }
    }
    
    if (move.adhesion_magnitude > STRONG_ADHESION_THRESHOLD) {
        // Strong adhesion reduces restitution (sticky collisions)
        move.restitution_coefficient *= (1.0 - move.adhesion_magnitude * 0.5);
    }
    
    return move;
}
```

### Force Combination Logic

#### 1. Movement Decision Algorithm
```cpp
void WorldB::queueMaterialMoves(double deltaTime) {
    for (uint32_t x = 0; x < width_; ++x) {
        for (uint32_t y = 0; y < height_; ++y) {
            CellB& cell = at(x, y);
            if (cell.getFillRatio() <= MIN_FILL_THRESHOLD) continue;
            
            // Calculate all acting forces
            Vector2d gravity_force(0.0, gravity_ * deltaTime * cell.getFillRatio());
            CohesionForce cohesion = calculateCohesionForce(x, y);
            AdhesionForce adhesion = calculateAdhesionForce(x, y);
            
            // FORCE COMBINATION LOGIC:
            
            // 1. Driving Forces (cause movement)
            Vector2d net_driving_force = gravity_force + adhesion.force_direction;
            
            // 2. Resistance Forces (oppose movement)  
            double total_resistance = cohesion.resistance_magnitude;
            
            // 3. Movement Threshold Check
            double driving_magnitude = net_driving_force.magnitude();
            
            if (driving_magnitude > total_resistance) {
                // 4. Calculate movement direction and strength
                Vector2d movement_direction = net_driving_force.normalized();
                double movement_strength = driving_magnitude - total_resistance;
                
                // 5. Scale movement by strength for realistic physics
                Vector2d scaled_velocity = movement_direction * movement_strength;
                cell.setVelocity(cell.getVelocity() + scaled_velocity);
                
                // 6. Standard boundary crossing detection
                Vector2d newCOM = cell.getCOM() + cell.getVelocity() * deltaTime;
                std::vector<Vector2i> crossed_boundaries = getAllBoundaryCrossings(newCOM);
                
                // 7. Create collision-aware moves with force data
                for (const Vector2i& direction : crossed_boundaries) {
                    Vector2i targetPos = Vector2i(x, y) + direction;
                    if (isValidCell(targetPos)) {
                        MaterialMove move = createCollisionAwareMove(
                            cell, at(targetPos), Vector2i(x, y), targetPos, direction, deltaTime
                        );
                        pending_moves_.push_back(move);
                    }
                }
            } else {
                // Movement blocked by resistance - material stays put
                spdlog::trace("Movement blocked: {} at ({},{}) - driving={:.3f} < resistance={:.3f}",
                             getMaterialName(cell.getMaterialType()), x, y, 
                             driving_magnitude, total_resistance);
            }
        }
    }
}
```

#### 2. Force Priority System
```cpp
// Force priority order:
// 1. GRAVITY: Always present, proportional to density and fill ratio
// 2. ADHESION: Attractive/repulsive forces between different materials
// 3. COHESION: Resistance threshold that must be overcome for movement
// 4. COLLISION: Only when movement occurs and boundaries are crossed

// Example force interactions:
// - WATER on DIRT: gravity(high) + adhesion_to_dirt(medium) > cohesion(low) → flows and sticks
// - METAL on METAL: gravity(high) + adhesion_to_metal(medium) < cohesion(very_high) → stays connected
// - LEAF in AIR: gravity(low) + adhesion_to_air(minimal) > cohesion(low) → floats/drifts easily
```

## Expected Behavioral Changes

### Water Improvements
- **Natural Pooling**: Low cohesion resistance (0.1) allows gravity + adhesion to drive horizontal spreading
- **Surface Tension**: Adhesion to WALL/DIRT creates meniscus effects at boundaries
- **Flow Dynamics**: Reduced cohesion enables realistic flow patterns around obstacles

### Material Differentiation
- **Granular Materials**: DIRT/SAND show pile formation with angle of repose based on cohesion thresholds
- **Rigid Materials**: METAL/WOOD maintain structural integrity through high cohesion resistance  
- **Fluid Materials**: WATER flows freely while maintaining surface effects from adhesion
- **Organic Materials**: LEAF shows low-cohesion, wind-like behavior with environmental interactions

## Implementation Order

1. **Force Calculation Infrastructure** (Phase 1)
2. **Movement Integration** (Phase 2)  
3. **Material-Specific Behaviors** (Phase 3)
4. **Testing & Validation** (Phase 4)
5. **Performance & Polish** (Phase 5)

**Total Estimated Time**: 7-11 hours of focused development

## Success Metrics

- ✅ **Water pools naturally** instead of staying in isolated cells
- ✅ **Metal particles stick** when they contact (no magnetic attraction)  
- ✅ **Sand forms piles** with realistic slopes
- ✅ **Wood beams maintain integrity** under reasonable forces
- ✅ **All materials behave distinctly** based on their cohesion/adhesion properties

## Integration with Existing Systems

### Collision System Compatibility
- **Enhanced MaterialMove**: Cohesion/adhesion data added to collision calculations
- **Force-Based Restitution**: Material binding affects bounce behavior
- **Transfer Resistance**: Cohesive materials resist fragmentation

### UI Integration  
- **Material Properties Display**: Show cohesion/adhesion values in material picker
- **Force Visualization**: Optional overlay showing binding forces
- **Parameter Controls**: Sliders for tuning force multipliers

---

**Status**: Ready for implementation
**Dependencies**: None (builds on existing collision and transfer systems)
**Risk Level**: Low (additive feature, doesn't break existing physics)
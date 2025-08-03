WorldB.  

## The Grid

WorldB is composed of a grid of square cells.

Each cell is from [0,1] full.

### Matter
It is filled with matter, of one of the following types: dirt, water, wood, sand, metal, air, leaf, wall, and nothing.

The matter is modeled by a single particle within each cell.
This is the cell's COM, or Center of Mass. The COM ranges from [-1, 1] in x and y.

-1 is the top or left. +1 is the bottom or right.

The boundries of the world are composed of wall blocks.
Wall blocks are a special, immobile kind of block that other blocks reflect off of.

Every type of matter has the following properties:

    elasticity
    cohesion
    adhesion
    density
    motion_coherence_threshold
    recovery_time
    
The simulation models:

    2d kinematics
    pressure
    density
    cohesion
    adhesion
    motion coherence

### Velocity
The matter in each cell moves according to 2D kinematics.

Velocity is limited to max of 0.9 cell per time step. This is to prevent skipping cells. On each timestemp, if V > 0.5, it is slowed down by %10. This is to spread out the velocity reduction.

The COM moves according to it's velocity.

When the particle crosses from inside a cell boundry to another cell, the matter will either transfer into the target cell, or it will reflect off of the shared boundary, or some of both. Matter is conserved during transfers. transfer_amount = min(source_amount, target_capacity)

The boundary is at [-0.99,0.99].

Reflections are handled as elastic collisions, taking into account the properties of each material.

## Transfers
When matter is transferred to the target cell, the matter is added with realistic physics:

**For empty target cells:**
- COM is calculated based on trajectory crossing the boundary, not reset to (0,0)
- Boundary crossing calculation:
  - Find intersection point of velocity vector with cell boundary
  - Transform crossing point to target cell coordinate space (-1 to +1)
  - Wrap coordinates across boundary (e.g., right boundary +1.0 becomes left side -1.0)
- Velocity is preserved through the transfer

**For non-empty target cells:**
- Momentum conservation: `new_COM = (m1*COM1 + m2*COM2) / (m1 + m2)`
- Incoming material COM calculated from boundary crossing trajectory (same as empty cell logic)
- Velocity momentum conservation: `new_velocity = (m1*v1 + m2*v2) / (m1 + m2)`

The goal is for COM transfer to follow a trajectory smoothly, reflecting and reacting with other cells in a believable way.

The system handles the problem of multiple moves targetting the same cell via a 2 step process:

1. Compute the possible moves and queue them up.
2. Attempt to apply the moves. Do this in random order. If there is space to move some of the matter, move it, otherwise treat the blocked matter it as an elastic collision, affecting the COM of both cell's accordingly.

## Gravity
The world has gravity. It comes from an imaginary point source that can be inside or outside the world. Gravity is a force applied to each Cell's COM.

## Motion Coherence

Motion Coherence detects when particles are moving as a coordinated group versus individually. This prevents artificial clumping of falling objects.

### Coherence Detection
- **Coherence Score**: 0.0 (chaotic motion) to 1.0 (identical motion)
- **Calculation**: Average velocity difference with neighbors, normalized by maximum expected velocity
- **Threshold**: Material-specific threshold for considering motion "coherent" (typically 0.7-0.9)

### Motion States
Each cell tracks its motion state:
- **STATIC**: Supported by surface, minimal velocity
- **FALLING**: No support, downward velocity > 0.1
- **SLIDING**: Moving along a surface with support
- **TURBULENT**: High velocity differences with neighbors (splashing, colliding)

State transitions occur when conditions change:
```
STATIC -> FALLING: Lost support
FALLING -> TURBULENT: Impact or high strain rate
TURBULENT -> STATIC: Velocity damped below threshold
```

**Hysteresis Prevention**: State transitions use different thresholds to prevent oscillation:
- STATIC → FALLING: velocity.y < -0.15
- FALLING → STATIC: velocity.y > -0.05
- Similar bands for other transitions

## Cohesion

Cohesion is a force that attracts/binds materials of the same type together (water is attracted to water). It is different than Adhesion.

### Motion-Aware Cohesion
Cohesion strength adapts based on motion state and coherence:

**State Multipliers**:
- STATIC: 1.0 (full cohesion maintains structure)
- FALLING with high coherence: 0.1 (minimal cohesion for falling groups)
- FALLING with low coherence: 0.5 (moderate cohesion)
- TURBULENT: 0.0 (no cohesion during splashing)

### Contact Stability
Cohesion strength also depends on contact duration:
- New contacts: 0% cohesion strength  
- After 5 timesteps: 50% strength
- After 10 timesteps: 100% strength
- Prevents artificial clumping during collisions

### Binding
This Cohesion provides resistance to Movement, modified by motion state.
- Purpose: Prevents material from moving away from connected neighbors
- Method: Calculates resistance thresholds based on same-material neighbor count and structural support
- Support Analysis:
    - Vertical Support: Checks for continuous material below (up to 5 cells) with recursive validation
    - Horizontal Support: Detects rigid high-density neighbors with strong mutual adhesion
- Motion Adjustment: Resistance multiplied by state-based cohesion multiplier
- Applied in: updateTransfers() - creates movement thresholds that particles must overcome

### COM Force
COM (Center-of-Mass) Cohesion - Attractive Forces.
- Purpose: Pulls particles toward the weighted center of connected neighbors.
- Method: Applies attractive forces directly to particle velocities.
- Range: Configurable neighbor detection range (typically 2 cells).
- Applied in: applyCohesionForces() - modifies velocity vectors each timestep.

### Continuous Resistance Model (WIP - PROTOTYPE)
**Status: Work in Progress - Experimental prototype implementation available**

Instead of binary threshold checks, cohesion provides velocity damping:
```
damping_factor = 1.0 + (cohesion_resistance * motion_state_multiplier)
velocity += net_force / damping_factor * deltaTime
```
This allows gradual deformation and smooth transitions between static and flowing states.

**Implementation Note**: A prototype is implemented in `WorldB::resolveForces()` with a static toggle `USE_CONTINUOUS_RESISTANCE` (currently set to `false`). Set to `true` to enable continuous damping behavior. Motion state multipliers are not yet integrated.

## Adhesion

Adhesion: Attractive forces between different material types (unlike cohesion which works on same materials)
Purpose: Simulates how different materials stick to each other (e.g., water adhering to dirt, materials sticking to walls)

Each material has an adhesion value (0.0-1.0):
AIR:   0.0  (no adhesion)
DIRT:  0.2  (moderate adhesion)
WATER: 0.5  (high adhesion - sticks to surfaces)
WOOD:  0.3  (moderate adhesion)
SAND:  0.1  (low adhesion - doesn't stick well)
METAL: 0.1  (low adhesion)
LEAF:  0.2  (moderate adhesion)
WALL:  1.0  (maximum adhesion - everything sticks to walls)

Each material has motion thresholds:
WATER: coherence_threshold=0.9, recovery_time=5   (flows easily, quick to reform)
SAND:  coherence_threshold=0.7, recovery_time=20  (granular, slow to settle)
DIRT:  coherence_threshold=0.6, recovery_time=30  (clumpy, slow to stabilize)
METAL: coherence_threshold=0.95, recovery_time=0  (rigid, instant connections)

Force Calculation

1. Neighbor Analysis: Checks all 8 adjacent cells for different materials
2. Mutual Adhesion: Uses geometric mean sqrt(material1.adhesion × material2.adhesion)
3. Distance Weighting: Adjacent neighbors (1.0) vs diagonal (0.707)
4. Fill Ratio: Force scales with both cells' fill ratios
5. Vector Accumulation: Sums all adhesive forces into net direction

Physics Integration

Applied in two phases:
1. Velocity Integration: Directly adds adhesive forces to particle velocities each timestep
2. Movement Decisions: Includes adhesion in net driving force calculations alongside gravity and cohesion

Key Differences from Cohesion
- Cohesion: Same-material clustering and resistance to separation
- Adhesion: Different-material attraction and surface sticking
- Cohesion: Calculated by WorldBCohesionCalculator with motion awareness
- Adhesion: Calculated by WorldBAdhesionCalculator

Behavioral Examples
- WATER + DIRT: Water particles attracted toward dirt surfaces (realistic wetting)
- Any Material + WALL: Strong attraction creates sticky boundary behavior
- METAL + WATER: Moderate adhesion (weak + strong)
- AIR interactions: No adhesive forces

  The system enables realistic multi-material physics where particles can form mixed structures through adhesive bonds between different material types.

## Pressure
Pressure is computed as both a hydrostatic force and a dynamic force.

    Stress vs Pressure

    Solids experience stress tensors (normal + shear components).
    Material Response For fluids, pressure causes flow. For solids, pressure causes compression. 
    Load Bearing Solids can support shear loads and create arches.
    A pile of dirt doesn't flow like water - it forms stable slopes... angle of repose for granular materials.

For arbitrary gravity direction, slices are perpendicular to gravity vector.

    Slice Orientation // For arbitrary gravity direction, slices are perpendicular to gravity vector Vector2d gravity_dir = normalize(gravity); Vector2d slice_normal = gravity_dir;

    Accumulation Formula float pressure[slice] = pressure[previous_slice] + density[slice] * gravity_magnitude * slice_thickness;

    Handle Partial Fills // Weight pressure by how full each cell is float effective_density = cell.fill_ratio * material_density[cell.type];

// Pseudo-code for solid pressure if (material == DIRT || material == SAND) { // Granular - acts somewhat fluid-like under high pressure apply_hydrostatic_pressure(); if (pressure > friction_threshold) { allow_flow(); } } else if (material == WOOD || material == METAL) { // Rigid - only compress, don't flow apply_compression_only(); }

Dynamic Pressure methods:
    
  1. Virtual Gravity Transfers (Static Pressure Buildup)

  - Generated by generateVirtualGravityTransfers() in WorldBPressureCalculator
  - Created even when cells have zero velocity
  - Energy = 0.5 × density × (gravity × deltaTime)²
  - Represents the constant downward force of gravity trying to compress material
  - Creates pressure when material can't move down due to obstacles

  2. Actual Collision/Movement Transfers (Impact Pressure)

  - Created when material with velocity actually collides with obstacles
  - Energy based on actual velocity: 0.5 × mass × velocity²
  - Represents kinetic energy converted to pressure on impact
  - Handled through the collision system and material moves

  The system combines both types:
  - Static buildup: Gravity constantly trying to push material down
  - Dynamic impacts: Actual movement and collisions

  This dual approach allows the system to model both:
  - Hydrostatic-like pressure (weight of material at rest)
  - Dynamic pressure waves (from impacts and movement)    
  
###  Pressure System Architecture

  1. Pressure Calculation Phase

  - Hydrostatic Pressure: Calculated by accumulating weight from top to bottom
  - Dynamic Pressure: Created from blocked transfers (both virtual gravity and actual collisions)

  2. Force Application Phase

  The pressure creates forces, not direct material transfers:
  // From applyPressureForces():
  Vector2d pressure_force = net_gradient * -1.0 * pressure_scale_ * deltaTime;
  cell.addPendingForce(pressure_force);

  3. Force Resolution Phase

  Forces accumulate and affect velocity:
  // In resolveForces():
  1. Clear pending forces
  2. Apply gravity forces
  3. Apply air resistance
  4. Apply pressure forces
  5. Apply cohesion forces
  6. Resolve against resistance

  4. Material Movement Phase

  Velocity (affected by pressure forces) creates material moves:
  // In computeMaterialMoves():
  Vector2d newCOM = cell.getCOM() + cell.getVelocity() * deltaTime;
  // Then checks if COM crosses cell boundaries to create MaterialMove

  Key Points:

  - Pressure doesn't directly move material
  - Pressure creates forces → Forces modify velocity → Velocity creates material moves
  - This indirect approach allows pressure to interact naturally with other forces (gravity, cohesion, etc.)
  - Material only moves when COM crosses cell boundaries due to accumulated velocity

### Pressure Diffusion

  The system implements material-specific pressure propagation to model how pressure spreads through different materials:

  **Diffusion Algorithm**:
  1. **Neighbor Analysis**: Checks 4 or 8 neighbors (configurable) for pressure differences
  2. **Pressure Flow**: Pressure flows from high to low pressure regions
  3. **Material Interface**: Uses harmonic mean of diffusion coefficients at material boundaries
  4. **Distance Scaling**: Diagonal neighbors weighted by 1/√2 for accurate distance modeling

  **Material Diffusion Properties** (from MaterialType.cpp):
  - WATER: 0.8 (high diffusion - pressure spreads quickly)
  - SAND: 0.3 (moderate diffusion)
  - DIRT: 0.2 (low diffusion - pressure spreads slowly)
  - WOOD: 0.1 (very low diffusion)
  - METAL: 0.05 (minimal diffusion)
  - LEAF: 0.4 (moderate diffusion)
  - WALL: 0.0 (no diffusion - acts as barrier)
  - AIR: 1.0 (maximum diffusion - no resistance)

  **Implementation Details**:
  ```cpp
  // Harmonic mean for material interfaces
  double interface_diffusion = 2.0 * diffusion_rate * neighbor_diffusion 
                              / (diffusion_rate + neighbor_diffusion + 1e-10);
  
  // Pressure flux calculation
  pressure_flux += interface_diffusion * pressure_diff;
  
  // Update pressure with time scaling
  new_pressure = current_pressure + pressure_flux * deltaTime * scale_factor;
  ```

  **Key Behaviors**:
  - Empty cells act as pressure sinks (pressure = 0)
  - Walls block pressure diffusion completely
  - Different materials create natural pressure gradients
  - Diffusion rate affects how quickly pressure equalizes
  - Works with unified pressure system (hydrostatic + dynamic)

### Pressure Wave Reflection

The pressure system implements proper wave reflection at rigid boundaries (walls) to model realistic pressure dynamics.

**Problem**: Without reflection, pressure waves that reach walls simply dissipate, which is unphysical. In reality, pressure waves bounce off rigid boundaries.

**Solution**: Track pressure flux attempting to flow into walls during diffusion, then reflect it back based on material properties.

#### Reflection Mechanism

When pressure diffuses through the grid, it naturally flows from high to low pressure regions. At wall boundaries:

1. **No-Flux Boundary**: Walls are treated as having equal pressure to prevent artificial drainage
2. **Potential Flux Tracking**: Calculate the pressure that "wants" to flow but can't due to the rigid boundary
3. **Reflection Application**: This blocked flux is reflected back into the source cell

```cpp
// During pressure diffusion:
if (neighbor.isWall()) {
    // Walls as perfect reflectors - no gradient exists
    // But calculate what flux WOULD flow based on diffusion rate
    double potential_flux = current_pressure * interface_diffusion;
    
    // Track this blocked flux for reflection
    wall_reflections[y][x].blocked_flux += potential_flux;
    continue;  // Skip normal diffusion
}

// After diffusion, apply reflections:
for (each cell with blocked flux) {
    // Material-specific reflection coefficient
    double reflection_coeff = calculateReflectionCoefficient(material_type);
    
    // Add reflected pressure back
    new_pressure[cell] += blocked_flux * reflection_coeff * deltaTime;
}
```

#### Physics Rationale

This approach models walls as **pressure mirrors**:
- Pressure waves hitting walls bounce back with intensity based on material elasticity
- Energy is conserved (reflected pressure ≤ incident pressure)
- Different materials reflect differently (water reflects more than sand)

The reflection coefficient depends on:
- **Material elasticity**: More elastic materials reflect more pressure
- **Material density**: Lighter materials reflect pressure more readily
- **Flux magnitude**: Very small pressure differences are damped to prevent noise

This creates realistic behaviors like:
- Pressure waves bouncing in confined spaces
- Standing wave patterns in closed containers
- Proper pressure buildup against rigid boundaries

### Pressure Gradient Calculation at Boundaries

The pressure gradient calculation determines the direction and magnitude of pressure-driven forces. Special handling is required at wall boundaries to create realistic flow patterns.

#### Flow Redistribution Method

When calculating pressure gradients near walls, the system uses a flow redistribution approach:

1. **First Pass - Pressure Assessment**:
   - Calculate pressure differences to all neighbors (including walls)
   - Identify which directions are blocked by walls
   - Track total pressure that would flow toward blocked directions

2. **Second Pass - Redistribution**:
   - For open directions: Apply direct pressure gradient
   - Redistribute blocked pressure to available directions
   - Weight redistribution by distance (diagonal directions use 1/√2)

```cpp
// Example: Cell with wall to the right
// First pass identifies:
//   - Left: open, pressure_diff = 0.2
//   - Right: WALL, pressure_diff = 0.5 (blocked)
//   - Up: open, pressure_diff = 0.1
//   - Down: open, pressure_diff = 0.1
//
// Second pass redistributes:
//   - Total blocked pressure = 0.5
//   - Open directions = 3
//   - Each open direction gets: 0.5 / 3 = 0.167 additional
//
// Final gradients:
//   - Left: 0.2 + 0.167 = 0.367
//   - Up: 0.1 + 0.167 = 0.267
//   - Down: 0.1 + 0.167 = 0.267
```

#### Physical Rationale

This approach models how fluid pressure naturally redistributes when encountering obstacles:
- Pressure that cannot flow through walls increases flow in available directions
- Creates realistic routing around obstacles
- Prevents artificial pressure buildup at boundaries
- Enables complex flow patterns through gaps and constrictions

#### Implementation Notes

- Uses 8-directional gradient calculation by default for smoother flow
- Normalizes final gradient by total number of directions (not just open ones)
- Prevents excessive forces by distributing blocked pressure evenly
- Works with both hydrostatic and dynamic pressure components

## Force Combination Logic & Threshold System

  The WorldB physics system uses a hierarchical force model where multiple forces combine to determine whether and how materials move:

  Force Hierarchy

  1. Driving Forces (cause movement)
    - Gravity: gravity_vector * material_density * fill_ratio * deltaTime
    - Adhesion: Vector sum of attractions to different adjacent materials
    - External Forces: User interactions, pressure gradients
  2. Resistance Forces (oppose movement)
    - Cohesion: Static threshold based on same-material neighbor count
    - Friction: Material-specific resistance to sliding

  Threshold Decision Logic

  // Force combination at each timestep
  Vector2d net_driving_force = gravity_force + adhesion_force + external_forces;
  double total_resistance = cohesion_resistance + friction_resistance;

  // Binary threshold check
  if (net_driving_force.magnitude() > total_resistance) {
      // Material can move - proceed with boundary crossing detection
      Vector2d movement_direction = net_driving_force.normalized();
      double excess_energy = net_driving_force.magnitude() - total_resistance;

      // Scale velocity by excess energy for realistic physics
      cell.velocity += movement_direction * excess_energy;
  } else {
      // Material is "stuck" - zero velocity and skip movement
      cell.velocity = Vector2d(0, 0);
  }

  Material-Specific Behaviors

  - WATER: Low cohesion (0.1) → flows easily under gravity alone
  - DIRT: Medium cohesion (0.4) → requires moderate force to separate
  - METAL: High cohesion (0.9) → strong resistance, stays connected
  - WOOD: High cohesion (0.7) + medium adhesion → structural integrity

  Key Design Principles

  1. Contact-Only Physics: Forces only act between adjacent cells (no action at distance)
  2. Static Thresholds: Cohesion acts like static friction - materials either move or don't
  3. Vector Addition: Multiple adhesion forces naturally cancel when balanced
  4. Energy Conservation: Excess force above threshold becomes kinetic energy

  This creates realistic material differentiation where WATER pools naturally, METAL clumps stick together, and DIRT forms stable piles - all from the same
  underlying force calculation system.

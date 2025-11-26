World Physics System

## The Grid

World is composed of a grid of square cells.

Each cell is from [0,1] full.

### Matter
It is filled with matter, of one of the following types: dirt, water, wood, sand, metal, air, leaf, wall, root, and seed.

**AIR is a real material**: Cells are always full of some material. "Empty" cells contain AIR with fill_ratio=1.0. AIR has extremely low density (0.001) and zero cohesion/adhesion, making it easily displaced by other materials through the swap system. This enables realistic displacement physics, air bubbles, and buoyancy effects.

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
    static_friction_coefficient
    kinetic_friction_coefficient
    stick_velocity
    friction_transition_width

The simulation models:

    2d kinematics
    pressure
    density
    cohesion
    adhesion
    motion coherence
    static and kinetic friction
    viscosity?  TODO update me

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

The system handles the problem of multiple moves targeting the same cell via a 2 step process:

1. Compute the possible moves and queue them up.
2. Attempt to apply the moves. Do this in random order. If there is space to move some of the matter, move it, otherwise treat the blocked matter it as an elastic collision, affecting the COM of both cell's accordingly.

## Gravity
The world has gravity. It comes from an imaginary point source that can be inside or outside the world. Gravity is a force applied to each Cell's COM.

## Motion Coherence

Motion Coherence detects when particles are moving as a coordinated group versus individually. This prevents artificial clumping of falling objects.

### Coherence Detection
**TODO: NOT YET IMPLEMENTED** - Currently only support-based state detection exists (STATIC vs FALLING).
- **Coherence Score**: 0.0 (chaotic motion) to 1.0 (identical motion)
- **Calculation**: Average velocity difference with neighbors, normalized by maximum expected velocity
- **Threshold**: Material-specific threshold for considering motion "coherent" (typically 0.7-0.9)

### Motion States
**PARTIALLY IMPLEMENTED** - Enum exists in WorldB.h:35, motion state multipliers work, but only STATIC/FALLING are actually assigned.
Each cell tracks its motion state:
- **STATIC**: Supported by surface, minimal velocity ✅ (implemented)
- **FALLING**: No support, downward velocity > 0.1 ✅ (implemented)
- **SLIDING**: Moving along a surface with support ❌ (TODO: not yet assigned)
- **TURBULENT**: High velocity differences with neighbors (splashing, colliding) ❌ (TODO: not yet assigned)

**TODO: Implement state transitions** - Currently only uses binary support check (WorldB.cpp:845).
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

## Viscosity

Viscosity represents a material's resistance to flow and deformation. Unlike the deprecated cohesion binding system, viscosity provides continuous damping rather than binary thresholds.

### Material Viscosity Values (0.0 - 1.0)
- **AIR**: 0.001 (negligible resistance)
- **WATER**: 0.01 (flows easily)
- **SAND**: 0.3 (granular flow resistance)
- **DIRT**: 0.5 (cohesive, resists flow)
- **LEAF**: 0.2 (light, some resistance)
- **WOOD**: 0.9 (essentially solid)
- **METAL**: 0.95 (rigid body)
- **WALL**: 1.0 (infinite - never flows)

### Physics Implementation
Viscosity creates continuous velocity damping:
```
damping_factor = 1.0 + (viscosity * fill_ratio * support_factor)
velocity += net_driving_force / damping_factor * deltaTime
```

Key behaviors:
- No binary thresholds - all forces create some movement
- Higher viscosity = more resistance to flow
- Natural pressure equilibration without oscillations
- Smooth transitions between static and flowing states

### Motion State Integration

Materials have a motion_sensitivity property (0.0-1.0) that determines how their viscosity is affected by motion state:

**Motion Sensitivity Values**:
- AIR: 0.0 (unaffected by motion)
- WATER: 1.0 (fully affected by motion state)
- SAND: 0.5 (moderately affected)
- DIRT: 0.7 (significantly affected)
- LEAF: 0.8 (highly affected)
- WOOD: 0.2 (slightly affected)
- METAL: 0.1 (barely affected)
- WALL: 0.0 (unaffected)

**Motion State Effects**:
When in different motion states, materials experience reduced viscosity:
- STATIC: 100% of base viscosity (no reduction)
- SLIDING: 50% of base viscosity
- FALLING: 30% of base viscosity
- TURBULENT: 10% of base viscosity

The actual viscosity multiplier is interpolated based on motion_sensitivity:
`effective_multiplier = 1.0 - sensitivity * (1.0 - state_multiplier)`

This allows rigid materials like METAL to maintain high viscosity even when falling, while fluids like WATER flow freely in all motion states.

## Air Resistance

Air resistance is a material-specific property that provides velocity-dependent damping. Unlike viscosity (which is internal flow resistance), air resistance models drag from the surrounding medium.

### Material Air Resistance Values (0.0 - 1.0)
- **AIR**: 0.0 (no self-drag)
- **WATER**: 0.01 (very low - fluid)
- **METAL**: 0.1 (low - dense, compact)
- **SAND**: 0.2 (low - small, heavy particles)
- **SEED**: 0.2 (low-moderate - compact, dense)
- **DIRT**: 0.3 (moderate - chunky particles)
- **WOOD**: 0.4 (moderate - shape-dependent)
- **LEAF**: 0.8 (high - large surface area, light)
- **WALL**: 0.0 (immobile, n/a)

### Physical Basis

In reality, all objects fall at the same rate in a vacuum (9.81 m/s²). The difference in falling speeds comes from air resistance, which depends on:
- Shape and cross-sectional area (drag coefficient)
- Material density (affects terminal velocity)
- Surface roughness

By making air resistance a material property, we correctly model why:
- Heavy, compact objects (METAL, SEED) fall quickly
- Light, flat objects (LEAF) fall slowly
- All objects experience the same gravitational acceleration

### Implementation

Air resistance is applied in `WorldAirResistanceCalculator.cpp`:
```cpp
double force_magnitude = strength * props.air_resistance * velocity_magnitude²
```

The force opposes motion and scales quadratically with velocity, creating realistic terminal velocity behavior.

## Friction (Static and Kinetic)

Friction provides velocity-dependent resistance that simulates the difference between static friction (resistance to start moving) and kinetic friction (resistance while moving). This creates realistic "stick-slip" behavior where materials resist initial movement but flow more freely once in motion.

### Friction Properties

Each material has four friction parameters:
- **static_friction_coefficient**: Resistance multiplier when at rest (typically 1.0-1.5)
- **kinetic_friction_coefficient**: Resistance multiplier when moving (typically 0.4-1.0)
- **stick_velocity**: Velocity below which full static friction applies (0.0-0.05)
- **friction_transition_width**: How quickly friction transitions from static to kinetic (0.02-0.1)

### Friction Calculation

The friction system uses a smooth transition function to avoid discontinuities:

```
friction_coefficient = getFrictionTransition(velocity_magnitude, material_params)

// Smooth transition from static to kinetic
if (velocity < stick_velocity) {
    return static_coefficient;
}
t = clamp((velocity - stick_velocity) / transition_width, 0, 1)
smooth_t = t * t * (3.0 - 2.0 * t)  // Smooth cubic interpolation
return lerp(static_coefficient, kinetic_coefficient, smooth_t)
```

### Material Friction Values

- **METAL**: stick=0.01, width=0.02, static=1.5, kinetic=1.0 (very sticky, sharp breakaway)
- **WOOD**: stick=0.02, width=0.03, static=1.3, kinetic=0.9 (sticky, moderate breakaway)
- **DIRT**: stick=0.05, width=0.10, static=1.0, kinetic=0.5 (resists flow, avalanches when moving)
- **SAND**: stick=0.04, width=0.08, static=0.6, kinetic=0.4 (light resistance, flows when disturbed)
- **LEAF**: stick=0.03, width=0.06, static=0.5, kinetic=0.3 (light materials, easy to move)
- **WATER**: stick=0.00, width=0.01, static=1.0, kinetic=1.0 (no static friction)
- **AIR**: stick=0.00, width=0.01, static=1.0, kinetic=1.0 (no friction)
- **WALL**: N/A (immobile)

### Integration with Forces

Friction combines with viscosity and support to create total resistance:

```
// Calculate velocity-dependent friction
velocity_magnitude = cell.velocity.magnitude()
friction_coefficient = getFrictionTransition(velocity_magnitude, material)

// Combine with viscosity and support
effective_viscosity = base_viscosity * friction_coefficient
total_damping = 1.0 + (effective_viscosity * fill_ratio * support_factor)

// Apply to forces
cell.velocity += net_driving_force / total_damping * deltaTime
```

### Physical Behaviors

The friction system creates several realistic behaviors:

1. **Static Equilibrium**: Materials at rest require sufficient force to overcome static friction
2. **Breakaway Motion**: Once moving, resistance drops to kinetic friction level
3. **Avalanche Effects**: Dirt/sand piles hold their shape until disturbed, then flow
4. **Stick-Slip**: Materials alternate between sticking and slipping on surfaces
5. **Support Dependency**: Friction only applies when material has structural support

### Pressure Interaction (Future Enhancement)

High pressure can reduce the effective stick velocity, making materials more likely to flow under compression:
```
effective_stick_velocity = base_stick_velocity * (1.0 - pressure_factor)
```

This creates realistic behavior where compressed materials flow more easily than uncompressed ones.

## Cohesion (Attractive Forces Only)

Cohesion now refers only to attractive forces between same-material particles.

### COM Force
COM (Center-of-Mass) Cohesion - Attractive Forces with two components:

1. **Clustering Force**: Pulls particles toward the weighted center of same-material neighbors.
   - Only active when same-material neighbors exist.
   - Strength scales with neighbor count and mass.

2. **Centering Force**: Pulls COM back toward cell center (0,0) for stability.
   - **Scaled by neighbor connectivity**: Isolated particles (zero same-material neighbors) have zero centering force, allowing free movement through space.
   - Particles with neighbors experience centering proportional to connection count.
   - Prevents artificial drag on projectiles moving through AIR.

- Method: Applies attractive forces directly to particle velocities.
- Range: Configurable neighbor detection range (typically 2 cells).
- Applied in: applyCohesionForces() - modifies velocity vectors each timestep.

## Adhesion

Adhesion: Attractive forces between different material types (unlike cohesion which works on same materials)
Purpose: Simulates how different materials stick to each other (e.g., water adhering to dirt, materials sticking to walls)

**AIR handling**: AIR neighbors are explicitly skipped in adhesion calculations since AIR has zero adhesion. This prevents unnecessary force computations for particles moving through AIR.

Each material has an adhesion value (0.0-1.0):
AIR:   0.0  (no adhesion - always skipped)
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

### Rigid Material Pressure Response

Materials marked as `is_rigid=true` (METAL, WOOD, WALL, SEED) do not respond to pressure gradients at all. This models the physical reality that solids transmit stress rather than flowing in response to pressure:

- **Fluids** (WATER, AIR): Fully respond to pressure gradients, flowing from high to low pressure.
- **Granular materials** (DIRT, SAND): Partially respond to pressure (scaled by `hydrostatic_weight`).
- **Rigid materials** (METAL, WOOD, WALL, SEED): Skip pressure force application entirely.

This separation allows viscosity to be used for its proper purpose (flow rate resistance) rather than as a hack to make solids behave solid-like.

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
  - Empty cells are no-flux boundaries (pressure stays in fluid, doesn't leak into air)
  - Walls block pressure diffusion completely
  - Different materials create natural pressure gradients
  - Diffusion rate affects how quickly pressure equalizes
  - Works with unified pressure system (hydrostatic + dynamic)
  - Minimum pressure change (0.5) allows zero-pressure cells to receive pressure via diffusion

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

### Pressure Gradient Improvements

Recent fixes enable proper pressure-driven flow:

1. **No early bailout** - Gradients calculated even for zero-pressure cells, allowing high-pressure neighbors to push into low-pressure regions
2. **Vertical gradient includes empty cells** - Empty cells above fluid create downward gradient (pressure 0 vs fluid pressure), enabling upward forces
3. **Combined with no-flux diffusion** - Pressure stays in fluid while gradient creates force toward empty space

This enables U-tube equalization and other pressure-driven vertical flow.

## Force Combination Logic

  The WorldB physics system uses continuous force integration with viscosity-based damping:

  Force Hierarchy

  1. Driving Forces (cause movement)
    - Gravity: gravity_vector (constant acceleration for all materials)
    - Adhesion: Vector sum of attractions to different adjacent materials
    - Cohesion: Attractive forces toward same-material neighbors
    - Pressure: Gradient-based forces from pressure differences
    - External Forces: User interactions

  2. Damping Forces (resist movement)
    - Air Resistance: Material-specific drag coefficient [0.0-1.0]
    - Viscosity: Continuous flow resistance based on material properties
    - Friction: Velocity-dependent resistance (static vs kinetic)
    - Support Factor: Modifies damping based on structural support

  Continuous Force Integration

  // Force combination at each timestep
  Vector2d net_driving_force = gravity_force + adhesion_force + cohesion_force + pressure_force + external_forces;

  // Calculate velocity-dependent friction
  double velocity_magnitude = cell.velocity.magnitude();
  double friction_coefficient = getFrictionTransition(velocity_magnitude, material);

  // Combine viscosity with friction
  double effective_viscosity = material.viscosity * friction_coefficient;

  // Continuous damping with friction (no thresholds)
  double damping_factor = 1.0 + (effective_viscosity * fill_ratio * support_factor);
  cell.velocity += net_driving_force / damping_factor * deltaTime;

  // Velocity always responds to forces - no binary "stuck" state

  Material-Specific Behaviors

  - WATER: Low viscosity (0.01) → flows easily, quick pressure equilibration
  - SAND: Moderate viscosity (0.3) → granular flow, forms natural slopes
  - DIRT: Higher viscosity (0.5) → cohesive clumps, slower flow
  - METAL: Very high viscosity (0.95) → essentially rigid, minimal flow
  - WOOD: High viscosity (0.9) → maintains structure, deforms only under extreme force

  Key Design Principles

  1. Contact-Only Physics: Forces only act between adjacent cells
  2. Continuous Response: All forces create proportional velocity changes
  3. Natural Damping: Viscosity prevents oscillations and unrealistic acceleration
  4. Smooth Transitions: No artificial thresholds or discontinuities

  This creates realistic material differentiation through viscosity and friction - WATER flows freely, METAL maintains rigid structures until sufficient force is applied, and granular materials form stable piles with natural angles of repose that avalanche when disturbed.

---

## Implementation Status

### Current State (As of pressure branch)

The system currently implements all features described above **except** for the static/kinetic friction system. The current implementation uses only viscosity-based damping without velocity-dependent friction coefficients.

**What's implemented:**
- Full viscosity system with material-specific values
- Motion states and motion sensitivity
- Support-based damping modulation
- All other physics systems (cohesion, adhesion, pressure, etc.)

**What's missing:**
- Static/kinetic friction coefficients in MaterialType
- Velocity-dependent friction calculation
- getFrictionTransition() function
- Integration of friction with viscosity in force resolution

### Implementation Plan

1. **Update MaterialType.h/cpp**
   - Add friction properties to MaterialProperties struct:
     ```cpp
     double static_friction_coefficient;
     double kinetic_friction_coefficient;
     double stick_velocity;
     double friction_transition_width;
     ```
   - Update material definitions with friction values

2. **Create Friction Calculator**
   - Add method to MaterialType or create utility function:
     ```cpp
     double getFrictionCoefficient(double velocity_magnitude) const;
     ```
   - Implement smooth cubic transition function

3. **Modify WorldB::resolveForces()**
   - Calculate velocity magnitude for each cell
   - Get friction coefficient from material
   - Multiply base viscosity by friction coefficient
   - Use combined value in damping calculation

4. **Testing**
   - Create visual test with slope at various angles
   - Verify static materials stay in place
   - Test breakaway behavior when force exceeds static friction
   - Validate avalanche effects for granular materials
   - Ensure WATER/AIR maintain no friction behavior

5. **Future Enhancement**
   - Add pressure-based stick velocity modification
   - Consider temperature effects on friction
   - Add surface-specific friction modifiers

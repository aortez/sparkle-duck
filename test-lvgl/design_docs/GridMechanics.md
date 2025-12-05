World Physics System

## The Grid

World is composed of a grid of square cells.

Each cell is from [0,1] full.

### Matter
It is filled with matter, of one of the following types: dirt, water, wood, sand, metal, air, leaf, wall, root, and seed.

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
    pressure (hydrostatic and dynamic)
    density
    cohesion
    adhesion
    motion coherence
    static and kinetic friction
    viscosity
    air resistance

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

**Note:** Coulomb friction only applies to non-fluid materials. Fluids (WATER, AIR) are excluded from the friction calculator, as they use viscosity to model internal flow resistance rather than surface friction.

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
- **DIRT**: stick=0.1, width=0.10, static=1.5, kinetic=0.5 (resists flow, avalanches when moving)
- **SAND**: stick=0.04, width=0.08, static=0.6, kinetic=0.4 (light resistance, flows when disturbed)
- **LEAF**: stick=0.03, width=0.06, static=0.5, kinetic=0.3 (light materials, easy to move)
- **WATER**: stick=0.0, width=0.001, static=0, kinetic=0 (fluids excluded from friction)
- **AIR**: stick=0.0, width=0.01, static=1.0, kinetic=1.0 (fluids excluded from friction)
- **WALL**: N/A (immobile)

### Integration with Forces

Friction is calculated as contact forces between adjacent non-fluid cells:

```
for each cardinal neighbor pair (cellA, cellB):
  normal_force = pressure_difference + weight (for vertical contacts)
  tangential_velocity = relative_velocity - normal_component
  friction_coefficient = getFrictionTransition(tangential_speed, propsA, propsB)

  friction_force = friction_coefficient * normal_force * friction_strength
  friction_direction = -tangential_velocity.normalize()

  cellA.addPendingForce(friction_force * friction_direction)
  cellB.addPendingForce(-friction_force * friction_direction)
```

Friction forces are accumulated during the friction calculation phase and applied alongside other forces (gravity, pressure, cohesion) during force resolution.

### Physical Behaviors

The friction system creates several realistic behaviors:

1. **Static Equilibrium**: Materials at rest require sufficient force to overcome static friction
2. **Breakaway Motion**: Once moving, resistance drops to kinetic friction level
3. **Avalanche Effects**: Dirt/sand piles hold their shape until disturbed, then flow
4. **Stick-Slip**: Materials alternate between sticking and slipping on surfaces
5. **Contact-Based**: Friction applies between adjacent non-fluid cells based on normal force (pressure + weight)

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
DIRT:  0.3  (moderate adhesion)
WATER: 0.3  (moderate adhesion)
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

The system uses a unified pressure field that combines pressure from multiple sources: gravity (weight), collisions (impacts), and diffusion (spreading).

### Pressure Sources

**1. Gravity Injection (Incremental)**
- Each fluid cell pushes its weight onto the cell below
- Calculated each frame: `pressure += density * gravity * hydrostatic_strength * deltaTime`
- Builds pressure incrementally over multiple frames
- Creates depth-dependent pressure naturally

**2. Collision Impacts**
- Blocked material transfers add pressure spikes
- Energy calculated from blocked momentum: `energy = velocity * blocked_mass`
- Scaled by material properties and `dynamic_strength`
- Creates transient pressure waves from impacts

**3. Pressure Diffusion**
- Spreads pressure from high to low pressure regions
- Multi-pass iteration for faster equilibration
- Material-specific diffusion rates
- Empty cells act as no-flux boundaries (pressure sealed in)

### Rigid Material Pressure Response

Materials marked as `is_rigid=true` (METAL, WOOD, WALL, SEED) do not respond to pressure gradients at all. This models the physical reality that solids transmit stress rather than flowing in response to pressure:

- **Fluids** (WATER, AIR): Fully respond to pressure gradients, flowing from high to low pressure.
- **Granular materials** (DIRT, SAND): Partially respond to pressure (scaled by `hydrostatic_weight`).
- **Rigid materials** (METAL, WOOD, WALL, SEED): Skip pressure force application entirely.

This separation allows viscosity to be used for its proper purpose (flow rate resistance) rather than as a hack to make solids behave solid-like.

### Pressure System Architecture

**Unified Pressure Field:**
Each cell has a single `pressure` value that combines all sources.

**Frame Processing Order:**
1. **Inject gravity pressure** - Each fluid cell adds weight to cell below
2. **Add collision pressure** - Blocked transfers from last frame add impact energy
3. **Diffuse pressure** - Multi-pass spreading (configurable iterations, default 2)
4. **Decay pressure** - Exponential decay (10% per second) prevents infinite buildup
5. **Calculate gradients** - Pressure differences create directional forces
6. **Apply forces** - Gradient forces added to pending force system
7. **Move material** - Velocity moves COM, detects new collisions for next frame

**Key Principles:**
- Pressure doesn't directly move material
- Pressure creates gradients → Gradients create forces → Forces modify velocity → Velocity moves COM
- All pressure sources combine in a single unified field
- Diffusion smooths internal gradients, reducing oscillation
- Decay prevents infinite pressure accumulation

### Pressure Diffusion

Material-specific pressure spreading using 8-neighbor diffusion (includes diagonals).

**Algorithm:**
1. Multi-pass iteration (configurable, default 2 iterations per frame)
2. Each iteration spreads pressure one cell-hop in all directions
3. Harmonic mean at material interfaces: `2 * r1 * r2 / (r1 + r2)`
4. Diagonal neighbors weighted by 1/√2 for distance
5. Stability limits prevent numerical explosion

**Material Diffusion Rates:**
- WATER: 0.9 (high - pressure spreads quickly)
- LEAF: 0.6, SAND: 0.3, DIRT: 0.3 (moderate)
- WOOD: 0.15, METAL: 0.1 (low - slow spreading)
- WALL: 0.0 (barrier), AIR: 1.0 (maximum)

**Boundary Conditions:**
- **Empty cells**: No-flux boundaries (pressure sealed in, doesn't leak to air)
- **Walls**: No-flux boundaries (pressure reflects)
- **World edges**: No-flux boundaries

**Multi-Pass Benefits:**
- Faster equilibration (more iterations = smoother pressure field)
- Reduces internal oscillation in blobs
- Allows tuning between wave-like (1 iter) and smooth (5 iters) behavior

### Pressure Gradient Calculation

Uses central difference method to calculate pressure gradients:

```cpp
// Horizontal: ∂P/∂x ≈ (P_right - P_left) / 2
// Vertical: ∂P/∂y ≈ (P_down - P_up) / 2
gradient = Vector2d(-(p_right - p_left) / 2.0, -(p_down - p_up) / 2.0)
```

**Boundary Handling:**
- Walls: Treated as same pressure (no gradient)
- Empty cells: Treated as same pressure (no gradient - sealed boundaries)
- Edge of world: Treated as same pressure (no gradient)

**Force Calculation:**
```cpp
pressure_force = gradient * pressure_scale * hydrostatic_weight
cell.addPendingForce(pressure_force)
```

Material-specific `hydrostatic_weight` determines how strongly each material responds to pressure gradients:
- Fluids (WATER): 1.0 (full response)
- Granular (SAND, DIRT): 0.25-0.5 (partial response)
- Rigid (WOOD, METAL): 0.0 (no response - transmit stress instead)

## Material Swaps

When two cells attempt to exchange positions (typically lighter material rising through heavier, or vice versa), the swap system uses momentum and energy checks to determine if the swap should occur.

### Swap Decision Process

1. **Momentum Check** (direction-dependent):
   - Horizontal: `from_momentum * penalty > to_resistance * threshold`
   - Vertical: `(from_momentum + buoyancy) > to_resistance * threshold`

2. **Energy Check**:
   - Available energy (collision + buoyancy) must exceed swap cost + bond breaking cost
   - Non-fluid swaps have higher energy cost (×10 default)

### Path of Least Resistance

To prevent unrealistic behavior (e.g., water "climbing" into cliff faces), the system checks if the displaced material has easier escape routes:

```
if (vertical_swap && target.is_fluid && target != AIR):
  for each lateral neighbor of target:
    if (neighbor.isEmpty()):
      deny_swap  // Fluid should escape sideways
    if (neighbor.pressure < target.pressure * 0.5):
      deny_swap  // Lower pressure path available
```

This allows pressure to redirect fluids laterally when blocked, creating realistic flow around obstacles rather than penetration through solids.

### Configurable Parameters

- `horizontal_non_fluid_penalty` (0.1): Momentum multiplier for fluid→solid horizontal swaps
- `horizontal_non_fluid_target_resistance` (5.0): Resistance multiplier for supported targets
- `horizontal_flow_resistance_factor` (0.5): Global threshold multiplier
- `non_fluid_energy_multiplier` (10.0): Energy cost multiplier for solid swaps

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
  - DIRT: Low viscosity (0.2) → granular flow with some cohesion
  - SAND: Moderate viscosity (0.3) → granular flow, forms natural slopes
  - METAL: Very high viscosity (1.0) → essentially rigid, minimal flow
  - WOOD: High viscosity (1.0) → maintains structure, deforms only under extreme force

  Key Design Principles

  1. Contact-Only Physics: Forces only act between adjacent cells
  2. Continuous Response: All forces create proportional velocity changes
  3. Natural Damping: Viscosity prevents oscillations and unrealistic acceleration
  4. Smooth Transitions: No artificial thresholds or discontinuities

  This creates realistic material differentiation through viscosity and friction - WATER flows freely, METAL maintains rigid structures until sufficient force is applied, and granular materials form stable piles with natural angles of repose that avalanche when disturbed.

---

## Implementation Status

### Current State

All major physics systems are implemented:

**Implemented:**
- ✅ Unified pressure field (incremental injection + collision impacts + diffusion)
- ✅ Multi-pass pressure diffusion with configurable iterations
- ✅ Viscosity system with material-specific values and motion sensitivity
- ✅ Coulomb friction (static/kinetic) with smooth transitions — applies to non-fluids only
- ✅ Cohesion (COM-based attractive forces for same-material particles)
- ✅ Adhesion (attractive forces between different materials)
- ✅ Air resistance (material-specific drag)
- ✅ Swap logic with momentum and energy checks
- ✅ Path-of-least-resistance swap filtering (prevents unrealistic cliff climbing)

**Known Limitations:**
- Motion coherence detection (STATIC/FALLING states exist, SLIDING/TURBULENT not yet assigned)
- Support system toggleable but physics designed to work without it via pressure-based resistance

**Recent Changes:**
- Unified pressure system (removed separate hydrostatic/dynamic components)
- Incremental gravity injection replaces column-based calculation
- Sealed boundaries (empty cells = no-flux, pressure doesn't leak to air)
- Multi-pass diffusion for faster equilibration and reduced oscillation

WorldB:

## The Grid

WorldB is composed of a grid of square cells.

Each cell is from [0,1] full.

### Matter
It is filled with matter, of one of the following types: dirt, water, wood, sand, metal, air, leaf, wall, and nothing.

The matter is modeled by a single particle within each cell.
This is the cell's COM, or Center of Mass. The COM ranges from [-1, 1] in x and y.

The boundries of the world are composed of wall blocks.
Wall blocks are a special, immobile kind of block that other blocks reflect off of.

Every type of matter has the following properties:

    elasticity
    cohesion
    adhesion
    density
    
The simulation models:

    2d kinematics
    pressure
    density
    cohesion
    adhesion

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
- Enhanced momentum conservation: `new_COM = (m1*COM1 + m2*COM2) / (m1 + m2)`
- Incoming material COM calculated from boundary crossing trajectory (same as empty cell logic)
- Velocity momentum conservation: `new_velocity = (m1*v1 + m2*v2) / (m1 + m2)`

The goal is for COM transfer to follow a trajectory smoothly, reflecting and reacting with other cells in a believable way.

The system handles the problem of multiple moves targetting the same cell via a 2 step process:

1. Compute the possible moves and queue them up.
2. Attempt to apply the moves. Do this in random order. If there is space to move some of the matter, move it, otherwise treat the blocked matter it as an elastic collision, affecting the COM of both cell's accordingly.

## Gravity
The world has gravity. It comes from an imaginary point source that can be inside or outside the world. Gravity is a force applied to each Cell's COM.

## Cohesion

Cohesion is a force that attracts/binds materials of the same type together (water is attracted to water).  It is different than Adhesion.

### Binding
This Cohesion provides resistance to Movement, if the resistance is enough, then the movement is prevented.
- Purpose: Prevents material from moving away from connected neighbors
- Method: Calculates resistance thresholds based on same-material neighbor count and structural support
- Support Analysis:
    - Vertical Support: Checks for continuous material below (up to 5 cells) with recursive validation
    - Horizontal Support: Detects rigid high-density neighbors with strong mutual adhesion
- Applied in: updateTransfers() - creates movement thresholds that particles must overcome

### COM Force
COM (Center-of-Mass) Cohesion - Attractive Forces.
- Purpose: Pulls particles toward the weighted center of connected neighbors.
- Method: Applies attractive forces directly to particle velocities.
- Range: Configurable neighbor detection range (typically 2 cells).
- Applied in: applyCohesionForces() - modifies velocity vectors each timestep.

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
- Cohesion: Calculated by WorldCohesionCalculator
- Adhesion: Calculated by WorldB::calculateAdhesionForce()

Behavioral Examples
- WATER + DIRT: Water particles attracted toward dirt surfaces (realistic wetting)
- Any Material + WALL: Strong attraction creates sticky boundary behavior
- METAL + WATER: Moderate adhesion (weak + strong)
- AIR interactions: No adhesive forces

  The system enables realistic multi-material physics where particles can form mixed structures through adhesive bonds between different material types.

## Pressure
Pressure is computed as a hydrostatic force.

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

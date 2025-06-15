WorldB:

WorldB is composed of a grid of square cells.

Each cell is from [0,1] full.

It is filled with matter, of one of the following types: dirt, water, wood, sand, metal, air, leaf, and wall.

The boundries of the world are composed of wall blocks.
Wall blocks are a special, immobile kind of block that other blocks reflect off of.

The matter in each cell moves according to 2D kinematics.

Velocity is limited to max of 0.9 cell per time step. This is to prevent skipping cells. On each timestemp, if V > 0.5, it is slowed down by %10. This is to spread out the velocity reduction.

The matter is modeled by a single particle within each cell.
This is the cell's COM, or Center of Mass. The COM ranges from [-1, 1] in x and y.

The COM moves according to it's velocity.

When the particle crosses from inside a cell boundry to another cell, the matter will either transfer into the target cell, or it will reflect off of the shared boundary, or some of both. Matter is conserved during transfers. transfer_amount = min(source_amount, target_capacity)

The boundary is at [-0.99,0.99].

Reflections are handled as elastic collisions, taking into account the properties of each material.

When matter is transferred to the target cell, the matter is added and its momentum is also added. new_COM = (m1COM1 + m2COM2) / (m1 + m2) The velocity of transferred matter should also be preserved.

The world has gravity. It comes from an imaginary point source that can be inside or outside the world. Gravity is a force applied to each Cell's COM.

Pressure is computed as a hydrostatic force.

    Stress vs Pressure

    Solids experience stress tensors (normal + shear components)
    Fluids only have hydrostatic pressure (normal forces only) 2. Material Response For fluids, pressure causes flow. For solids, pressure causes compression. 3. Load Bearing
        Solids can support shear loads and create arches
        A pile of dirt doesn't flow like water - it forms stable slopes
        angle of repose for granular materials

For arbitrary gravity direction, slices are perpendicular to gravity vector.

    Slice Orientation // For arbitrary gravity direction, slices are perpendicular to gravity vector Vector2d gravity_dir = normalize(gravity); Vector2d slice_normal = gravity_dir;

    Accumulation Formula float pressure[slice] = pressure[previous_slice] + density[slice] * gravity_magnitude * slice_thickness;

    Handle Partial Fills // Weight pressure by how full each cell is float effective_density = cell.fill_ratio * material_density[cell.type];

// Pseudo-code for solid pressure if (material == DIRT || material == SAND) { // Granular - acts somewhat fluid-like under high pressure apply_hydrostatic_pressure(); if (pressure > friction_threshold) { allow_flow(); } } else if (material == WOOD || material == METAL) { // Rigid - only compress, don't flow apply_compression_only(); }

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

The system handles the problem of multiple moves targetting the same cell via a 2 step process:

    Compute the possible moves and queue them up.
    Attempt to apply the moves. We'll do this in random order. If there is space to move some of the matter, move it, otherwise treat it as a reflection, affecting the COM of both cell's accordingly.


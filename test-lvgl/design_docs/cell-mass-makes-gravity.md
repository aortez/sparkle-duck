# Cell Mass as Gravity Source

## Overview

Transform gravity from a uniform downward force to an n-body system where each cell's mass creates a gravitational field. Every cell with mass attracts every other cell proportionally to their masses and inversely to distance squared.

## Physics Model

### Current System
- Uniform gravity: `F = density * g * direction_down`
- Applied in `World::applyGravity()`

### Proposed System
- N-body gravity: `F = G * m1 * m2 / r² * direction`
- Each cell is both a source and receiver of gravitational forces
- Configurable gravity modes: Classic (current), Radial (point source), Mass-based, Hybrid

## Implementation Architecture

### WorldGravityCalculator
New calculator class following existing pattern:
```cpp
class WorldGravityCalculator {
    void calculateGravityForces(World& world, double deltaTime);
    void buildQuadtree(World& world);
    Vector2d calculateNearField(Cell& cell, World& world);
    Vector2d calculateFarField(Cell& cell, QuadNode* node);
};
```

### Optimization Strategy

**Hybrid Near/Far Field Approach:**
- **Near field** (r < 5-10 cells): Direct O(n) calculation for accuracy
- **Far field** (r > 10 cells): Barnes-Hut quadtree approximation O(log n)
- **Theta parameter**: 0.7 default (tune for accuracy vs performance)

**Quadtree Integration:**
- Build once per timestep
- Store total_mass and center_of_mass per node
- Reuse for all force calculations in that frame

## Expected Behaviors

- **Accretion**: Materials naturally clump into spherical shapes
- **Orbital mechanics**: Possible with careful velocity initialization
- **Tidal effects**: Large masses affect nearby fluids differently
- **Gravity wells**: Dense materials (METAL) create stronger local fields

## Performance Considerations

- **Distance cutoff**: Ignore gravity beyond max_radius (e.g., 50 cells)
- **Mass threshold**: Skip cells with mass < epsilon
- **Spatial hashing**: Alternative to quadtree for uniform mass distributions
- **GPU acceleration**: Embarrassingly parallel for near-field calculations

## Configuration

```cpp
struct GravitySettings {
    enum Mode { CLASSIC, RADIAL, MASS_BASED, HYBRID };
    Mode mode = CLASSIC;
    double G = 0.01;           // Gravitational constant (tune for grid scale)
    double near_radius = 10.0;  // Cells to calculate directly
    double far_theta = 0.7;     // Barnes-Hut accuracy parameter
    double max_radius = 50.0;   // Absolute cutoff distance
};
```

## Integration Points

1. Replace `World::applyGravity()` with `gravity_calculator_.calculateGravityForces()`
2. Add gravity mode toggle to PhysicsControls UI
3. Extend PhysicsSettings with GravitySettings
4. Add gravity field visualization (optional debug feature)

## Open Questions

- How do WALL cells interact? (Infinite mass? Zero mass? Configurable?)
- Should AIR have tiny mass or true zero?
- Energy conservation strategy for orbital mechanics?
- Interaction with existing pressure system?
# Physics Optimization Ideas

## Overview
This document explores algorithmic optimizations to reduce computation in the cell-based physics simulation.

## 1. Bit-Packed Activity Tracking

### Concept
Use 64-bit integers as ultra-fast binary lookup maps. Each bit represents one cell's state.

```cpp
uint64_t active_cells;  // 64 cells in one integer
bool is_active = (active_cells >> cell_index) & 1;  // Single bit test
```

### Benefits
- **Memory**: 1 bit vs 176 bytes per cell = 1408× reduction
- **Speed**: Check 64 cells with one CPU instruction
- **Cache**: Entire grid's activity fits in L2 cache

### 8×8 Block Mapping (Chess Bitboard Inspired)
Each uint64_t represents an 8×8 spatial block of cells, similar to computer chess bitboards. This approach leverages decades of optimization from chess engines:

```cpp
// Each 64-bit word represents an 8×8 block spatially:
// Bit 0-7:   Row 0 of block
// Bit 8-15:  Row 1 of block
// Bit 56-63: Row 7 of block

// Check entire 8×8 block with single operation
if (activity_block == 0) skip_entire_block();

// Apply chess bitboard techniques:
// - Kogge-Stone fill for pressure/gravity flow
// - Magic bitboards for neighbor lookups
// - Parallel prefix operations for cumulative effects
// - Bit scan (ctz/clz) to find active cells quickly
```

**Why 8×8 blocks are superior for physics:**
- Spatial locality: neighbors often in same word
- Natural fit for local physics operations
- Enables chess-engine optimizations (sliding attacks → pressure flow)
- Hardware-friendly (popcount, bit scan instructions)

### Multiple Activity Layers
- `has_matter` - Non-empty cells
- `is_moving` - Velocity above threshold
- `has_pressure` - Pressure gradients present
- `needs_physics` - Requires processing this frame
- `neighbor_activity` - Has active neighbors (for wake-up)

## 2. Unified Neighbor Processing

### Problem
Currently 5-7 separate neighbor access passes per cell:
- CohesionCalculator
- AdhesionCalculator
- ViscosityCalculator
- PressureCalculator
- SupportCalculator

### Solution
Single-pass neighbor cache with pre-computed aggregates:

```cpp
struct NeighborCache {
    // Raw neighbor data
    MaterialType materials[8];
    float fill_ratios[8];
    Vector2f velocities[8];

    // Pre-computed aggregates
    uint8_t same_material_count;
    Vector2f avg_velocity;
    float max_pressure_diff;
    bool has_vertical_support;
};
```

One pass gathers everything, then all calculators use cached data.

## 3. Quiet Region Detection

### State Machine for Regions
```cpp
enum RegionState {
    ACTIVE,     // Full physics every frame
    SETTLING,   // Reduce update frequency (every 2-3 frames)
    QUIET,      // No updates until disturbed
    FROZEN      // Long-term static, fully cached
};
```

### Activity Propagation
When a cell becomes active, wake up neighbors in expanding rings:
- Immediate neighbors: Wake immediately
- 2-cell radius: Wake next frame
- 3+ cell radius: Mark as "potentially active"

### Hierarchical Activity Maps
```cpp
Level 0: Individual cells (64 per uint64_t)
Level 1: 8×8 blocks (64 blocks per uint64_t)
Level 2: 64×64 regions (64 regions per uint64_t)
```

Check coarse level first - if region inactive, skip entirely.

## 4. Temporal Coherence Caching?

### Frame-to-Frame Caching
For slow-moving materials, reuse previous frame's calculations:

```cpp
if (velocity < 0.01 && !neighbors_changed) {
    return cached_force * 0.95;  // Slight decay
}
```

### Support Map Caching?
Support rarely changes - only recalculate when:
- Material added/removed below
- Significant movement detected
- Every N frames as safety check

## 5. Sparse Data Structures

### Spatial Hashing for Particles
For particle-based operations (trees, organisms):
```cpp
unordered_map<uint32_t, vector<CellIndex>> spatial_hash;
uint32_t hash = (y / CHUNK_SIZE) * GRID_WIDTH + (x / CHUNK_SIZE);
```

## 6. Computation Reduction Strategies

### Early Exit Conditions
- Skip if force below threshold: `if (force.magnitude() < 0.001) continue;`
- Skip if material immobile: `if (material == WALL) continue;`
- Skip if region quiet: `if (region.quiet_frames > 60) continue;`

### Approximate Physics for Distant Effects
- Full physics for immediate neighbors
- Simplified physics for 2-cell radius
- Statistical approximation beyond that

### LOD (Level of Detail) System
```cpp
switch(distance_from_viewport) {
    case NEAR: update_every_frame();
    case MEDIUM: update_every_3_frames();
    case FAR: update_every_10_frames();
    case OFF_SCREEN: freeze();
}
```

## 7. Smart Update Scheduling

### Priority Queue for Updates
```cpp
priority_queue<Region, UpdatePriority> update_queue;
// High priority: User interaction area, moving materials
// Low priority: Static regions, off-screen areas
```

### Adaptive Timesteps
- Active regions: Full timestep
- Settling regions: Larger timestep (2-3× normal)
- Quiet regions: No update

## 9. Algorithm-Specific Optimizations

### Pressure Diffusion
- Use Red-Black Gauss-Seidel for better parallelization
- Multigrid solver for large grids
- Compress pressure to 16-bit float for non-critical cells

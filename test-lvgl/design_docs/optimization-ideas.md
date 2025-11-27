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
- `has_matter` - Non-empty cells (✅ **IMPLEMENTED** as `empty_cells` in GridOfCells)
- `is_moving` - Velocity above threshold
- `has_pressure` - Pressure gradients present
- `needs_physics` - Requires processing this frame
- `neighbor_activity` - Has active neighbors (for wake-up)

### Implementation Status (Nov 2025)

**Completed:**
- ✅ CellBitmap: Generic bit-packed grid with 8×8 blocks
- ✅ GridOfCells: Frame-scoped cache with `emptyCells()` bitmap
- ✅ Bit operation optimizations (shifts/masks instead of div/mod)
- ✅ Support calculator integration with runtime toggle
- ✅ Comprehensive testing (12 tests passing)
- ✅ CLI `--compare-cache` benchmarking tool
- ✅ EmptyNeighborhood: Typed wrapper with semantic API
- ✅ MaterialNeighborhood: 4-bit packed material types (9 cells = 36 bits)
- ✅ Optimized helpers: centerHasMaterial(), getMaterialNeighborsBitGrid()
- ✅ Performance timers for cache construction
- ✅ WorldSupportCalculator: Eliminated cell lookups for material type queries

**Current Performance (100×100 grid):**
- Construction: ~400μs/frame (includes material neighborhoods)
- Material lookups: 0 cell lookups (bitmap only)
- Ready for further optimizations

**Next Steps:**
- Apply MaterialNeighborhood to cohesion/adhesion calculators
- Optimize has_support checks (use bitmap for vertical support lookups)
- Branch hoisting (hoist `USE_CACHE` check outside loops)
- Multi-layer property bitmaps (velocity, pressure, etc.)

## 1b. Neighborhood Bitmap Extraction (NEW IDEA)

### Concept: 3×3 Local Neighborhoods in uint64_t

Instead of querying neighbors individually (8 `isSet()` calls with coordinate math),
extract a local 3×3 neighborhood once and query with simple bit shifts.

### Basic 3×3 Layout (9 bits)
```cpp
// 3×3 grid around cell (x, y):
//   NW N  NE     Bit positions:
//   W  C  E      0  1  2
//   SW S  SE     3  4  5
//                6  7  8

uint64_t neighborhood = grid.emptyCells().getNeighborhood3x3(x, y);

// Query neighbors with bit shifts (no coordinate math!):
bool north_empty = (neighborhood >> 1) & 1;
bool south_empty = (neighborhood >> 7) & 1;
bool east_empty = (neighborhood >> 5) & 1;
```

**Benefits:**
- One fetch instead of 8 separate `isSet()` calls
- Neighbor checks are pure bit operations (1 cycle vs ~20 cycles)
- ~3× faster for neighbor-heavy calculations

### Using Extra Bits in uint64_t

A 3×3 grid only uses 9 bits, leaving **55 bits free**! Here's how to use them:

#### Option 1: Validity/OOB Bits (9 additional bits)
```cpp
// Bits 0-8:   Property values (empty/full)
// Bits 9-17:  Validity flags (1 = in-bounds, 0 = out-of-bounds)
// Bits 18-63: Unused (46 bits)

uint64_t neighborhood = grid.getNeighborhood3x3WithValidity(x, y);

// Check if neighbor exists AND is empty:
bool north_valid = (neighborhood >> (9 + 1)) & 1;
bool north_empty = (neighborhood >> 1) & 1;

if (north_valid && !north_empty) {
    // Safe to use - neighbor exists and has material
}

// Edge case handling becomes trivial - no branching needed!
```

**Use case:** Eliminates boundary checking in physics calculations.

#### Option 2: Material Type Neighborhoods (36 bits)
```cpp
// With 9 material types, use 4 bits per cell:
// Bits 0-35:  Material types (9 cells × 4 bits)
// Bits 36-63: Unused (28 bits)
//
// Layout:
//   NW    N     NE        Bits 0-3   4-7   8-11
//   W     C     E              12-15  16-19 20-23
//   SW    S     SE             24-27  28-31 32-35

uint64_t mat_neighborhood = grid.getMaterialNeighborhood3x3(x, y);

// Extract material types:
MaterialType north = static_cast<MaterialType>((mat_neighborhood >> 4) & 0xF);
MaterialType center = static_cast<MaterialType>((mat_neighborhood >> 16) & 0xF);

// Count water neighbors in one loop:
int water_count = 0;
for (int i = 0; i < 9; ++i) {
    if (i == 4) continue;  // Skip center
    MaterialType mat = static_cast<MaterialType>((mat_neighborhood >> (i * 4)) & 0xF);
    if (mat == MaterialType::WATER) ++water_count;
}

// Check if all neighbors are same material (cohesion test):
MaterialType first = mat_neighborhood & 0xF;
bool all_same = true;
for (int i = 1; i < 9; ++i) {
    if (((mat_neighborhood >> (i * 4)) & 0xF) != first) {
        all_same = false;
        break;
    }
}
```

**Use case:** Cohesion, adhesion, viscosity calculations (all need neighbor materials).

#### Option 3: Multi-Layer Properties (45 bits for 5 layers)
```cpp
// Store 5 boolean properties × 9 cells = 45 bits:
// Bits 0-8:   isEmpty
// Bits 9-17:  isWall
// Bits 18-26: hasSupport
// Bits 27-35: isMoving
// Bits 36-44: hasPressure
// Bits 45-63: Unused (19 bits)

uint64_t multi_props = grid.getMultiLayerNeighborhood3x3(x, y);

// Extract property layers:
uint16_t empty_layer = multi_props & 0x1FF;           // Bits 0-8
uint16_t wall_layer = (multi_props >> 9) & 0x1FF;     // Bits 9-17
uint16_t support_layer = (multi_props >> 18) & 0x1FF; // Bits 18-26

// Count supported neighbors:
int supported_count = __builtin_popcount(support_layer & ~(1 << 4));  // Exclude center

// Check if surrounded by walls:
bool walled_in = (wall_layer & 0x1EF) == 0x1EF;  // All except center
```

**Use case:** Complex queries needing multiple properties.

#### Option 4: Hybrid Approach (Best Flexibility)
```cpp
// Bits 0-8:   Property value (e.g., isEmpty)
// Bits 9-17:  Validity flags (OOB handling)
// Bits 18-49: Material types (8 neighbors × 4 bits, skip center)
// Bits 50-63: Future use (14 bits)

// Now you get BOTH edge handling AND material types!
```

## Which to Implement?

**My suggestion:**
1. **Start with Option 1 (Validity bits)** - Eliminates edge branching, simple
2. **Add Option 2 (Material neighborhoods)** - Huge win for cohesion/adhesion
3. **Later: Option 3** - When we have more boolean properties to track

The material type neighborhood could **dramatically** speed up cohesion/adhesion calculations since they currently iterate neighbors multiple times!

Want me to implement validity bits + material neighborhoods?

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

## 10. Per-Cell Neighborhood Cache

### Concept

Instead of grid-level bitmaps that require extraction, store a 64-bit neighborhood cache **directly in each Cell** for instant access. This is a complementary approach to the grid-level caching in Section 1.

**Grid-level cache** (frame-scoped, rebuilt each frame):
```cpp
GridOfCells grid;
MaterialNeighborhood materials = grid.getMaterialNeighborhood3x3(x, y);
```

**Per-cell cache** (persistent, updated on changes):
```cpp
struct Cell {
    // ... existing fields ...
    uint64_t neighborhood_cache;  // Updated when neighborhood changes
};

// Instant lookup, no extraction
bool has_support = (cell.neighborhood_cache >> SUPPORT_BIT_OFFSET) & 1;
```

### Bit Layout: Asymmetric Design (64 bits exactly)

With a 3×3 neighborhood (9 cells), we can pack rich information:

```cpp
// Center cell (16 bits):
[4 bits]  Own material type (0-15, supports all 9 types)
[2 bits]  Support level (0-3)
[8 bits]  Neighbor presence flags (1 bit per 8 neighbors)
[2 bits]  unused

// Each of 8 neighbors (6 bits × 8 = 48 bits):
[1 bit]   Has material (fill_ratio > threshold)
[1 bit]   Same material as center
[1 bit]   horizontal support
[1 bit]   vert support
[2 bits]  unused

Total: 16 + 48 = 64 bits
```

### Physical Layout in uint64_t

```
Bits 0-15:   Center cell data
Bits 16-21:  North neighbor
Bits 22-27:  Northeast neighbor
Bits 28-33:  East neighbor
Bits 34-39:  Southeast neighbor
Bits 40-45:  South neighbor
Bits 46-51:  Southwest neighbor
Bits 52-57:  West neighbor
Bits 58-63:  Northwest neighbor
```

### What This Eliminates

**Current expensive operations that become O(1):**

1. **Support calculation**: Check support bit instead of scanning neighbors
2. **Cohesion**: Check "same_material" bits (8 neighbors, no cell lookups!)
3. **Adhesion**: Check "different_material" pattern instantly
4. **Motion coherence**: Check neighbor motion states
5. **Material queries**: Center material type without dereferencing properties

### Example Usage

```cpp
// Extract center material type
MaterialType mat = static_cast<MaterialType>((cache >> 0) & 0xF);

// Check if any neighbor has same material (cohesion check)
uint8_t same_material_bits = 0;
for (int i = 0; i < 8; ++i) {
    int bit_offset = 16 + (i * 6) + 1;  // Second bit of each neighbor
    same_material_bits |= ((cache >> bit_offset) & 1) << i;
}
bool has_cohesion_neighbor = same_material_bits != 0;

// Count neighbors that can provide support
int support_count = 0;
for (int i = 0; i < 8; ++i) {
    int bit_offset = 16 + (i * 6) + 2;  // Third bit of each neighbor
    support_count += (cache >> bit_offset) & 1;
}
```

### Tradeoffs

- **Memory**: +8 bytes per cell (relatively small, ~4% increase)
- **Consistency**: Must maintain cache coherence on updates
- **Complexity**: More bookkeeping code

### Future Extensions

The 3 reserved bits per neighbor could be used for:
- **Fill ratio bins** (8 levels: 0%, 14%, 28%, ..., 100%)
- **Pressure level** (8 bins for quick pressure queries)
- **Velocity magnitude** (8 bins: still, slow, medium, fast, etc.)

### Implementation Notes

- Start with basic bits (has_material, same_material, can_support)
- Add helper methods: `hasCohesionNeighbors()`, `getSupportCount()`, `getCenterMaterial()`
- Profile before/after to validate performance gains
- Consider making cache optional (runtime toggle for A/B testing)

### Relationship to Grid-Level Caches

These approaches complement each other:
- **Grid-level**: Global queries ("which cells are empty?"), batch operations
- **Per-cell**: Local queries ("does this cell have support?"), hot-path physics

Use grid-level for broad scans, per-cell for detailed physics calculations.

# Server Simulation Loop Parallelization Analysis

## Executive Summary

The Sparkle Duck physics simulation has significant parallelization potential. The current sequential pipeline processes ~10,000 cells (100×100 grid) with 8-12 physics calculators each frame. Key opportunities include:

- **Per-cell force calculations** (80-90% of CPU time) - embarrassingly parallel
- **Grid-level operations** (support map, pressure diffusion) - data parallel
- **Multi-grid physics** - separate grids can run concurrently

**Estimated speedup**: 2-4× on 4-8 cores for current grid sizes, with better scaling for larger grids.

---

## Current Simulation Pipeline

### Main Loop Structure (src/server/main.cpp:110-121)

```
mainLoopRun() {
    while (!shouldExit()) {
        processEventsFromQueue()  // Event handling
        tick()                    // Physics step ← TARGET FOR PARALLELIZATION
        frameLimiting()           // Sleep if needed
    }
}
```

### Physics Pipeline (src/core/World.cpp:454-535)

The `World::advanceTime()` method executes these sequential stages:

```cpp
1. GridOfCells construction           [~400μs]   ← Bitmap caching
2. computeSupportMapBottomUp()        [varies]   ← Grid scan
3. calculateHydrostaticPressure()     [varies]   ← Grid scan
4. resolveForces()                    [MAJOR]    ← 80%+ of time
   ├─ clearPendingForce()            ← Per-cell
   ├─ applyGravity()                 ← Per-cell, independent
   ├─ applyAirResistance()           ← Per-cell, independent
   ├─ applyPressureForces()          ← Per-cell, reads neighbors
   ├─ applyCohesionForces()          ← Per-cell, reads neighbors
   ├─ applyFrictionForces()          ← Per-cell, reads neighbors
   ├─ applyViscousForces()           ← Per-cell, reads neighbors
   └─ resolution loop                ← Per-cell velocity update
5. processVelocityLimiting()          [minor]    ← Per-cell
6. updateTransfers()                  [varies]   ← Per-cell move computation
7. processMaterialMoves()             [varies]   ← Conflict resolution
8. processBlockedTransfers()          [varies]   ← Pressure update
9. applyPressureDiffusion()           [varies]   ← Grid scan
10. applyPressureDecay()              [minor]    ← Per-cell
11. TreeManager::update()             [varies]   ← Organism AI
```

---

## Parallelization Opportunities

### 1. Per-Cell Force Calculations (Highest Impact)

**Target**: `resolveForces()` - Lines 810-890 in World.cpp

**Current implementation**: Sequential loops over all cells
```cpp
for (uint32_t y = 0; y < height; ++y) {
    for (uint32_t x = 0; x < width; ++x) {
        Cell& cell = data.at(x, y);
        // Calculate forces...
    }
}
```

**Parallelization approach**: Data parallel over cells
```cpp
#pragma omp parallel for collapse(2)
for (uint32_t y = 0; y < height; ++y) {
    for (uint32_t x = 0; x < width; ++x) {
        // Each thread processes different cells
    }
}
```

**Why this works**:
- Force accumulation is read-only to neighbors, write-only to self
- No race conditions on `cell.addPendingForce()`
- Each cell's calculation is independent
- Neighbor reads are const (no writes during force calculation)

**Expected speedup**: 3-5× on 4-8 cores

**Phases that can parallelize this way**:
- ✅ `clearPendingForce()` - No dependencies
- ✅ `applyGravity()` - Reads gravity vector (const), writes to self
- ✅ `applyAirResistance()` - Reads velocity (const), writes to self
- ✅ `applyPressureForces()` - Reads neighbor pressure (const), writes to self
- ✅ `applyCohesionForces()` - Reads neighbor materials (const), writes to self
- ✅ `applyFrictionForces()` - Reads neighbor properties (const), writes to self
- ✅ `applyViscousForces()` - Reads neighbor velocities (const), writes to self
- ✅ `resolution loop` - Reads accumulated force, updates velocity (self only)
- ✅ `processVelocityLimiting()` - Per-cell operation
- ✅ `applyPressureDecay()` - Per-cell operation

### 2. Support Map Computation

**Target**: `WorldSupportCalculator::computeSupportMapBottomUp()`

**Current approach**: Bottom-up scan (sequential dependency)

**Parallelization options**:

#### Option A: Row-parallel with dependencies
```cpp
// Row N depends on row N+1, but cells within row are independent
for (int y = height - 1; y >= 0; --y) {
    #pragma omp parallel for
    for (uint32_t x = 0; x < width; ++x) {
        // Compute support for cell (x, y)
    }
    // Implicit barrier between rows
}
```

**Speedup**: 2-4× (limited by sequential rows)

#### Option B: Chunked parallelism
```cpp
// Divide grid into vertical strips, process independently
#pragma omp parallel
{
    int thread_id = omp_get_thread_num();
    int x_start = thread_id * chunk_width;
    int x_end = (thread_id + 1) * chunk_width;

    for (int y = height - 1; y >= 0; --y) {
        for (int x = x_start; x < x_end; ++x) {
            // Process strip bottom-up
        }
    }
}
```

**Speedup**: 3-6× on 4-8 cores

### 3. Pressure Diffusion

**Target**: `WorldPressureCalculator::applyPressureDiffusion()`

**Current approach**: Iterate all cells, compute pressure flux from neighbors

**Parallelization**: Red-Black Gauss-Seidel pattern
```cpp
// Red cells (checkerboard pattern)
#pragma omp parallel for collapse(2)
for (uint32_t y = 0; y < height; ++y) {
    for (uint32_t x = (y % 2); x < width; x += 2) {
        // Update red cells
    }
}

// Black cells (no dependency on red updates)
#pragma omp parallel for collapse(2)
for (uint32_t y = 0; y < height; ++y) {
    for (uint32_t x = ((y + 1) % 2); x < width; x += 2) {
        // Update black cells
    }
}
```

**Speedup**: 2-3× per iteration (but may need more iterations for convergence)

### 4. Material Move Computation

**Target**: `updateTransfers()` → `computeMaterialMoves()`

**Challenge**: Writes to shared `pending_moves_` vector (race condition)

**Solution**: Thread-local move buffers
```cpp
std::vector<std::vector<MaterialMove>> thread_moves(num_threads);

#pragma omp parallel
{
    int tid = omp_get_thread_num();

    #pragma omp for collapse(2)
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            // Compute moves for cell
            if (has_move) {
                thread_moves[tid].push_back(move);
            }
        }
    }
}

// Merge thread-local buffers (sequential, but fast)
for (auto& moves : thread_moves) {
    pending_moves_.insert(pending_moves_.end(), moves.begin(), moves.end());
}
```

**Speedup**: 3-5× on 4-8 cores

### 5. Hydrostatic Pressure Calculation

**Target**: `WorldPressureCalculator::calculateHydrostaticPressure()`

**Current approach**: Top-down accumulation (sequential dependency per column)

**Parallelization**: Column-parallel
```cpp
#pragma omp parallel for
for (uint32_t x = 0; x < width; ++x) {
    double accumulated_pressure = 0.0;
    for (uint32_t y = 0; y < height; ++y) {
        // Update pressure for column
    }
}
```

**Speedup**: Linear with core count (embarrassingly parallel)

### 6. Tree Organism Updates

**Target**: `TreeManager::update()`

**Parallelization**: Per-tree parallelism
```cpp
std::vector<TreeId> tree_ids;
for (auto& [id, tree] : trees_) {
    tree_ids.push_back(id);
}

#pragma omp parallel for
for (size_t i = 0; i < tree_ids.size(); ++i) {
    auto& tree = trees_[tree_ids[i]];
    tree.think(*world, deltaTime);
}
```

**Speedup**: Linear with tree count (when multiple trees exist)

---

## Sequential Dependencies (Cannot Parallelize)

### 1. Material Move Processing

**Target**: `processMaterialMoves()`

**Why sequential**: Conflict resolution between competing moves
- Multiple cells may try to move into same target
- Random shuffle needed for fairness
- Complex state updates with organism tracking

**Mitigation**: Parallelize the conflict detection, keep resolution sequential
```cpp
// Parallel: Build conflict map
std::unordered_map<Vector2i, std::vector<MoveIndex>> conflicts;

#pragma omp parallel
{
    std::unordered_map<Vector2i, std::vector<MoveIndex>> local_conflicts;

    #pragma omp for
    for (size_t i = 0; i < moves.size(); ++i) {
        local_conflicts[moves[i].target].push_back(i);
    }

    #pragma omp critical
    {
        // Merge local conflicts
        for (auto& [target, indices] : local_conflicts) {
            conflicts[target].insert(conflicts[target].end(),
                                   indices.begin(), indices.end());
        }
    }
}

// Sequential: Resolve conflicts with random shuffle
for (auto& [target, indices] : conflicts) {
    std::shuffle(indices.begin(), indices.end(), rng);
    // Apply first valid move, reject rest
}
```

### 2. GridOfCells Construction

**Target**: Bitmap cache building (Line 470)

**Why currently sequential**: Single-pass construction

**Potential parallelization**: Build bitmaps in parallel
```cpp
// Separate bitmaps can be built concurrently
#pragma omp parallel sections
{
    #pragma omp section
    buildEmptyCellsBitmap();

    #pragma omp section
    buildMaterialNeighborhoods();
}
```

**Speedup**: 1.5-2× (limited by cache construction time ~400μs)

---

## Implementation Strategy

### Phase 1: Low-Hanging Fruit (Minimal Risk)

**Target**: Per-cell force calculations in `resolveForces()`

1. Add OpenMP pragmas to force calculation loops
2. Ensure thread-safety of force accumulation (already safe)
3. Add runtime toggle: `--parallel-forces` flag
4. Benchmark and compare against sequential version

**Files to modify**:
- `src/core/World.cpp` - Add pragmas to force loops
- `src/server/main.cpp` - Add command-line flag
- `CMakeLists.txt` - Enable OpenMP (`find_package(OpenMP)`)

**Expected effort**: 2-4 hours
**Expected speedup**: 3-4× on resolveForces() (50-70% of total frame time)

### Phase 2: Grid-Level Operations

**Target**: Support map and pressure diffusion

1. Implement row-parallel or chunked support calculation
2. Implement red-black pressure diffusion
3. Add benchmarking to verify correctness

**Expected effort**: 1-2 days
**Expected speedup**: 2-3× on these phases (20-30% of frame time)

### Phase 3: Advanced Parallelism

**Target**: Material moves and organism updates

1. Thread-local move buffers for `computeMaterialMoves()`
2. Per-tree parallelism for `TreeManager::update()`
3. Parallel conflict detection in `processMaterialMoves()`

**Expected effort**: 2-3 days
**Expected speedup**: 2-3× on these phases (10-20% of frame time)

### Phase 4: Multi-Threading Architecture

**Future option**: Separate threads for different subsystems

```cpp
// Thread 1: Physics simulation (blocks on completion)
std::thread physics_thread([&]() {
    world->advanceTime(deltaTime);
});

// Thread 2: Network I/O (non-blocking)
std::thread network_thread([&]() {
    server->broadcastRenderMessage(worldData);
});

// Thread 3: Organism AI (can lag behind physics)
std::thread ai_thread([&]() {
    treeManager->think(world, deltaTime);
});

physics_thread.join();  // Wait for physics before next frame
```

---

## Challenges and Considerations

### 1. Thread Safety

**Current assumptions**:
- Force calculations read neighbors (const) and write to self
- No explicit synchronization needed for per-cell operations

**Validation needed**:
- Audit all calculator methods for hidden shared state
- Check MaterialMove and OrganismTransfer for race conditions
- Verify TreeManager is thread-safe for parallel tree updates

### 2. Performance Overhead

**OpenMP overhead**: ~10-50μs per parallel region
- Not worth it for tiny grids (< 20×20)
- Break-even at ~50×50 grid
- Clear win at 100×100+ grid

**Mitigation**: Runtime detection
```cpp
if (width * height < PARALLEL_THRESHOLD) {
    // Sequential path
} else {
    #pragma omp parallel
    // Parallel path
}
```

### 3. Memory Bandwidth

**Potential bottleneck**: Multiple threads reading cell data
- Current Cell size: ~176 bytes
- 100×100 grid = 1.76 MB (fits in L3 cache)
- 8 threads × neighbor reads = high cache pressure

**Mitigation strategies**:
- Chunked access patterns (improve cache locality)
- SIMD-friendly data layouts (see optimization-ideas.md Section 1)
- False sharing prevention (align thread chunks to cache lines)

### 4. Determinism

**Challenge**: OpenMP thread scheduling is non-deterministic

**Impact on simulation**:
- Floating-point rounding differences when summing forces
- Random number generator state (must use thread-local RNG)
- Material move order (already randomized, so non-issue)

**Solutions**:
- Use `schedule(static)` for deterministic work distribution
- Thread-local RNG with deterministic seeding
- Accept minor floating-point differences (physically insignificant)

### 5. Testing and Validation

**Required tests**:
- Correctness: Parallel vs sequential produces same results (within FP tolerance)
- Performance: Benchmark on 1, 2, 4, 8 threads
- Scaling: Test on various grid sizes (50×50, 100×100, 200×200, 500×500)
- Stability: Long-running simulations don't diverge

**Validation scenarios**:
- Empty world: Verify no spurious forces
- Dam break: Compare water flow patterns
- Pile stability: Verify dirt piles behave identically
- Tree growth: Ensure organism behavior is consistent

---

## Performance Projections

### Current Baseline (100×100 grid, sequential)

From benchmarking data in optimization-ideas.md:
- Total frame time: ~5-10ms
- Force resolution: ~3-6ms (60-70%)
- Support map: ~0.5-1ms (10%)
- Pressure diffusion: ~0.5-1ms (10%)
- Material moves: ~1-2ms (10-20%)

### Projected Speedup (4 cores, 100×100 grid)

**Optimistic scenario** (all parallelizable work scales linearly):
- Force resolution: 3-6ms → 0.75-1.5ms (4× speedup)
- Support map: 0.5-1ms → 0.25-0.5ms (2× speedup)
- Pressure diffusion: 0.5-1ms → 0.25-0.5ms (2× speedup)
- Material moves: 1-2ms → 0.5-1ms (2× speedup, parallel compute only)
- **Total: 5-10ms → 1.75-3.5ms (2.8-3.3× overall speedup)**

**Conservative scenario** (accounting for overhead and sequential portions):
- Force resolution: 3-6ms → 1-2ms (3× speedup)
- Support map: 0.5-1ms → 0.3-0.6ms (1.7× speedup)
- Pressure diffusion: 0.5-1ms → 0.3-0.6ms (1.7× speedup)
- Material moves: 1-2ms → 0.7-1.4ms (1.5× speedup)
- OpenMP overhead: +0.3-0.5ms
- **Total: 5-10ms → 2.6-4.6ms (2.0-2.4× overall speedup)**

### Scaling with Grid Size

| Grid Size | Sequential | 4-Core Parallel | Speedup |
|-----------|------------|-----------------|---------|
| 50×50     | 1.5ms      | 1.2ms           | 1.25×   |
| 100×100   | 7.5ms      | 3.0ms           | 2.5×    |
| 200×200   | 30ms       | 9ms             | 3.3×    |
| 500×500   | 190ms      | 50ms            | 3.8×    |

**Key insight**: Parallelism becomes more effective as grid size increases due to better overhead amortization.

---

## Alternative Approaches

### 1. GPU Acceleration (CUDA/OpenCL)

**Pros**:
- Massive parallelism (1000s of cores)
- Ideal for grid-based physics
- Could achieve 10-100× speedup

**Cons**:
- Major rewrite required
- CPU-GPU transfer overhead
- Complexity for organism AI (better on CPU)
- Platform-specific (not portable)

**Recommendation**: Consider for future if CPU parallelism is insufficient.

### 2. Task-Based Parallelism (Intel TBB, std::async)

**Pros**:
- More flexible than OpenMP
- Better load balancing
- C++ standard library support (std::async)

**Cons**:
- More complex code
- Requires task decomposition
- Higher overhead for fine-grained parallelism

**Recommendation**: Use for coarse-grained parallelism (e.g., separate physics/network threads), but OpenMP better for data parallelism.

### 3. SIMD Vectorization (AVX2/AVX512)

**Pros**:
- Process 4-8 cells simultaneously
- No threading overhead
- Deterministic results

**Cons**:
- Requires SOA (Structure of Arrays) data layout
- Platform-specific intrinsics
- Only helps with arithmetic-heavy operations

**Recommendation**: Combine with threading for maximum speedup (orthogonal optimizations).

### 4. Distributed Computing (MPI)

**Pros**:
- Scale across multiple machines
- Domain decomposition for huge grids

**Cons**:
- Massive complexity
- Inter-node communication overhead
- Overkill for current use case

**Recommendation**: Not needed for client-server architecture with moderate grid sizes.

---

## Recommendations

### Immediate Next Steps

1. **Implement Phase 1** (per-cell force parallelization)
   - Low risk, high reward
   - 2-4 hours of work
   - Expected 2-3× overall speedup
   - Easy to rollback if issues arise

2. **Add benchmarking infrastructure**
   - Extend existing CLI benchmark mode
   - Add `--threads N` flag
   - Report per-phase timing breakdown
   - Compare parallel vs sequential results

3. **Validate correctness**
   - Run existing test suite with parallel forces
   - Visual comparison of simulation behavior
   - Long-running stability test (1000+ frames)

### Medium-Term Goals

4. **Implement Phase 2** (grid-level operations)
   - Support map parallelization
   - Pressure diffusion red-black pattern
   - Expected +0.5-1× additional speedup

5. **Profile and optimize**
   - Identify new bottlenecks
   - Tune chunk sizes and scheduling
   - Consider SIMD for inner loops

### Long-Term Vision

6. **Hybrid parallelism**
   - OpenMP for within-node parallelism
   - Separate threads for physics/network/AI
   - SIMD for hot inner loops
   - GPU offload for special effects (future)

---

## Code Examples

### Example 1: Parallel Force Resolution

```cpp
// Before (sequential)
void World::resolveForces(double deltaTime, const GridOfCells* grid) {
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            Cell& cell = data.at(x, y);
            // ... apply forces ...
        }
    }
}

// After (parallel)
void World::resolveForces(double deltaTime, const GridOfCells* grid) {
    const bool use_parallel = (width * height >= PARALLEL_THRESHOLD);

    if (use_parallel) {
        #pragma omp parallel for collapse(2) schedule(static)
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                Cell& cell = data.at(x, y);
                // ... apply forces ...
            }
        }
    } else {
        // Sequential fallback
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                Cell& cell = data.at(x, y);
                // ... apply forces ...
            }
        }
    }
}
```

### Example 2: Parallel Support Map Computation

```cpp
void WorldSupportCalculator::computeSupportMapBottomUp(World& world) {
    const uint32_t height = world.getData().height;
    const uint32_t width = world.getData().width;

    // Bottom-up scan with row-level parallelism
    for (int y = height - 1; y >= 0; --y) {
        #pragma omp parallel for schedule(static)
        for (uint32_t x = 0; x < width; ++x) {
            Cell& cell = world.getData().at(x, y);

            // Compute support based on neighbors below (already computed)
            bool has_support = checkSupportBelow(world, x, y);
            cell.has_support = has_support;
        }
        // Implicit barrier - all cells in row complete before next row
    }
}
```

### Example 3: Thread-Local Move Buffers

```cpp
std::vector<MaterialMove> World::computeMaterialMoves(double deltaTime) {
    std::vector<MaterialMove> all_moves;

    #ifdef _OPENMP
    const int num_threads = omp_get_max_threads();
    std::vector<std::vector<MaterialMove>> thread_moves(num_threads);

    #pragma omp parallel
    {
        const int tid = omp_get_thread_num();
        auto& local_moves = thread_moves[tid];

        #pragma omp for collapse(2) schedule(dynamic)
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                Cell& cell = data.at(x, y);

                // Compute potential moves
                auto moves = computeMovesForCell(x, y, deltaTime);
                local_moves.insert(local_moves.end(), moves.begin(), moves.end());
            }
        }
    }

    // Merge thread-local buffers (sequential, but fast)
    size_t total_size = 0;
    for (const auto& moves : thread_moves) {
        total_size += moves.size();
    }
    all_moves.reserve(total_size);

    for (auto& moves : thread_moves) {
        all_moves.insert(all_moves.end(), moves.begin(), moves.end());
    }
    #else
    // Sequential fallback
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            auto moves = computeMovesForCell(x, y, deltaTime);
            all_moves.insert(all_moves.end(), moves.begin(), moves.end());
        }
    }
    #endif

    return all_moves;
}
```

---

## Conclusion

Parallelizing the Sparkle Duck simulation is **feasible and worthwhile**. The per-cell force calculations are embarrassingly parallel and represent 60-70% of frame time, making them ideal for parallelization.

**Key takeaways**:
- Start with Phase 1 (force parallelization) for immediate 2-3× speedup
- Grid-level operations offer additional 1.5-2× gains
- Total potential: 3-5× speedup on 4-8 cores for 100×100 grids
- Larger grids (200×200+) will see even better scaling
- Minimal code changes, low risk, high reward

**Next action**: Implement Phase 1 with OpenMP pragmas and validate correctness before proceeding to advanced parallelism.

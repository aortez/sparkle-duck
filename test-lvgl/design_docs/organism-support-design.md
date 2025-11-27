# Organism Support System Design

## Problem

WOOD cells in trees are falling because:
1. They have low density (0.3) and low adhesion (0.3)
2. Main support system uses RIGID_DENSITY_THRESHOLD (5.0) and STRONG_ADHESION_THRESHOLD (0.5)
3. WOOD fails both checks → no horizontal support → cells fall

## Solution: Two-Part Fix

### Part 1: Fix Main Support System (Non-Organism Materials)

**Current Problem:**
- `hasHorizontalSupport()` checks density > 5.0 AND adhesion > 0.5
- Uses adhesion (sticking force) instead of cohesion (structural bonding)
- Doesn't consider is_rigid flag properly

**New Approach:**
```cpp
bool hasHorizontalSupport(cell, neighbors) {
    for each neighbor {
        // Skip if neighbor isn't rigid
        if (!neighbor.material().is_rigid) continue;

        // Same material: use cohesion
        if (neighbor.material == cell.material) {
            if (cell.material().cohesion > COHESION_SUPPORT_THRESHOLD) {
                return true;
            }
        }
        // Different materials: use adhesion
        else {
            double mutual_adhesion = sqrt(cell.adhesion * neighbor.adhesion);
            if (mutual_adhesion > ADHESION_SUPPORT_THRESHOLD) {
                return true;
            }
        }
    }
    return false;
}
```

**Thresholds:**
- COHESION_SUPPORT_THRESHOLD = 0.5 (same material bonding strength)
- ADHESION_SUPPORT_THRESHOLD = 0.5 (different material sticking strength)

**Material Properties Affected:**
- WOOD: is_rigid=true, cohesion=0.7 → WOOD-to-WOOD support works!
- METAL: is_rigid=true, cohesion=1.0 → METAL-to-METAL support works!
- DIRT: is_rigid=false → DIRT doesn't provide horizontal support (correct!)

### Part 2: Organism Support (Realistic Tree Physics)

**Goal:** Trees are supported if connected to anchored points (ground, trunk).

**Algorithm:**

1. **Find Anchor Points** (cells that are inherently supported):
   - SEED cells (always anchored - tree core)
   - Organism cells touching ground (y = height - 1)
   - Organism cells with vertical support from non-organism materials

2. **Flood Fill from Anchors**:
   - BFS/DFS through cardinally-adjacent same-organism cells
   - Mark all reachable cells as organism-supported
   - Unreachable cells = disconnected branches → no organism support

3. **Set Support Flags**:
   - Reachable cells: `has_any_support = true`
   - Unreachable cells: keep existing support (may still have regular horizontal support)

**Realistic Behaviors:**
- ✅ Tree rooted to ground → entire tree supported
- ✅ Floating seed → falls (no anchor)
- ✅ Branch severed from trunk → falls (disconnected)
- ✅ Tree on platform → supported if SEED/ROOT touches platform

**Implementation Location:**
```cpp
class TreeManager {
    void computeOrganismSupport(World& world);

private:
    // Mark organism cells connected to anchored points
    void floodFillSupport(
        World& world,
        TreeId tree_id,
        std::unordered_set<Vector2i>& supported_cells);
};
```

**Called from World::advanceTime():**
```cpp
// After main support calculation
{
    ScopeTimer timer(pImpl->timers_, "organism_support");
    if (tree_manager_) {
        tree_manager_->computeOrganismSupport(*this);
    }
}
```

## Expected Results

### Before Fix:
- WOOD[0] at (2,2): has_any_support=0 → FALLING → falls
- WOOD[1] at (3,2): has_any_support=1 (vertical from SEED below)

### After Fix (Main Support):
- WOOD[0] at (2,2): has horizontal support from WOOD[1] (cohesion=0.7 > 0.5)
- Both WOOD cells: has_any_support=1

### After Fix (Organism Support):
- SEED at (3,3): anchored (on ground)
- ROOT at (3,4): connected to SEED → supported
- WOOD[1] at (3,2): connected to SEED → supported
- WOOD[0] at (2,2): connected to WOOD[1] → supported
- All cells: has_any_support=1 → STATIC motion state → full viscosity → no falling!

## Testing Strategy

1. **Test broken support calculation**:
   - Two WOOD cells side-by-side
   - Verify they support each other via cohesion

2. **Test organism anchoring**:
   - Seed on ground → tree grows → all cells supported
   - Seed in air → tree grows → all cells fall together

3. **Test branch disconnection**:
   - Manually remove WOOD cell connecting branch to trunk
   - Verify disconnected branch falls

4. **Test multi-organism**:
   - Two separate trees
   - Verify cells only get support from their own organism_id

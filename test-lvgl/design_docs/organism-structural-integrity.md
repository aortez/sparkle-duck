# Organism Structural Integrity Design

## The Insight

**Use existing organism infrastructure!** Trees already have:
- `organism_id` tracking which cells belong to which tree
- Seed cell (the "root" of the tree)
- TreeManager tracking all trees

## Core Algorithm: Flood-Fill + Velocity Sync

### Each Physics Timestep:

1. **IDENTIFY CONNECTED STRUCTURE**
   ```
   For each tree in TreeManager:
       Start flood-fill from tree.seed_position
       Find all cells with matching organism_id that are reachable
       Mark as "connected"
   ```

2. **PRUNE DISCONNECTED FRAGMENTS**
   ```
   For all cells with this organism_id:
       If NOT marked as connected:
           cell.organism_id = 0  // Remove from organism
           // Cell becomes independent particle
   ```

3. **APPLY FORCES NORMALLY**
   - All physics runs as usual (gravity, cohesion, friction, etc.)
   - Forces accumulate, F=ma creates velocities
   - Disconnected cells behave as normal particles

4. **SYNCHRONIZE VELOCITIES**
   ```
   For each tree:
       avg_velocity = mass_weighted_average(all connected cell velocities)

       For each connected cell:
           cell.velocity = avg_velocity
   ```
   Result: Tree moves as rigid unit

## Implementation

### TreeManager::syncOrganismStructures()

```cpp
void TreeManager::syncOrganismStructures(World& world) {
    WorldData& data = world.getData();

    for (auto& [tree_id, tree] : trees_) {
        // 1. Find connected cells via flood fill from seed
        std::vector<Vector2i> connected =
            findConnectedCells(world, tree_id, tree.seed_position);

        // 2. Prune disconnected cells
        for (auto& pos : tree.cells) {
            if (!contains(connected, pos)) {
                Cell& cell = data.at(pos.x, pos.y);
                if (cell.organism_id == tree_id) {
                    cell.organism_id = 0;  // Fragment breaks off
                    spdlog::info("Tree {} fragment at ({},{}) disconnected",
                                 tree_id, pos.x, pos.y);
                }
            }
        }

        // 3. Update tree's cell list
        tree.cells = connected;

        // 4. Calculate mass-weighted average velocity
        Vector2d velocity_sum(0, 0);
        double total_mass = 0;

        for (auto& pos : connected) {
            Cell& cell = data.at(pos.x, pos.y);
            double mass = cell.getMass();
            velocity_sum += cell.velocity * mass;
            total_mass += mass;
        }

        Vector2d avg_velocity = (total_mass > 0) ? velocity_sum / total_mass : Vector2d(0,0);

        // 5. Apply averaged velocity to all connected cells
        for (auto& pos : connected) {
            data.at(pos.x, pos.y).velocity = avg_velocity;
        }
    }
}
```

### Flood Fill Helper

```cpp
std::vector<Vector2i> TreeManager::findConnectedCells(
    World& world,
    uint32_t organism_id,
    Vector2i seed_pos) {

    std::vector<Vector2i> connected;
    std::queue<Vector2i> to_visit;
    std::set<std::pair<int,int>> visited;

    to_visit.push(seed_pos);
    visited.insert({seed_pos.x, seed_pos.y});

    WorldData& data = world.getData();

    while (!to_visit.empty()) {
        Vector2i pos = to_visit.front();
        to_visit.pop();

        // Check bounds
        if (pos.x < 0 || pos.y < 0 ||
            pos.x >= (int)data.width || pos.y >= (int)data.height) {
            continue;
        }

        Cell& cell = data.at(pos.x, pos.y);

        // Must belong to this organism
        if (cell.organism_id != organism_id) {
            continue;
        }

        connected.push_back(pos);

        // Check 4 cardinal neighbors (or 8 for diagonal connections)
        static const std::array<Vector2i, 4> neighbors = {{
            {-1, 0}, {1, 0}, {0, -1}, {0, 1}
        }};

        for (auto& offset : neighbors) {
            Vector2i neighbor_pos = { pos.x + offset.x, pos.y + offset.y };
            auto key = std::make_pair(neighbor_pos.x, neighbor_pos.y);

            if (!visited.count(key)) {
                visited.insert(key);
                to_visit.push(neighbor_pos);
            }
        }
    }

    return connected;
}
```

### Integration Point

In `World::updatePhysics()`, after force resolution:

```cpp
// After resolveForces():
resolveForces(deltaTime);

// NEW: Synchronize organism structures
if (tree_manager_ && settings.organism_structural_integrity) {
    tree_manager_->syncOrganismStructures(*this);
}

// Continue with movement:
processVelocityLimiting(deltaTime);
computeMaterialMoves(deltaTime);
```

## Benefits

✅ **Uses existing infrastructure** - organism_id, seed tracking, TreeManager
✅ **Automatic fragmentation** - damaged trees naturally break into pieces
✅ **Simple implementation** - just flood fill + velocity averaging
✅ **Works with existing physics** - no new force calculations
✅ **Emergent behavior** - disconnected fragments fall realistically
✅ **Trees maintain shape** - horizontal branches don't sag

## Limitations

❌ **No rotation** - whole tree can't tip over as unit (yet)
❌ **No local flex** - tree is rigid (might feel stiff)
❌ **Only organisms** - doesn't help generic rigid materials (metal piles)
❌ **Performance** - flood fill every frame (but should be fast for small trees)

## Examples

### Example 1: Healthy Tree
```
        [SEED]
          |
        [WOOD]
          |
    [WOOD]-[WOOD]-[WOOD]-[WOOD]  ← horizontal branch

Flood fill finds all 6 cells connected to SEED
All get same velocity → branch stays horizontal
```

### Example 2: Damaged Tree
```
Before:                  After removing middle WOOD:
    [SEED]                   [SEED]
      |                        |
    [WOOD]                   [WOOD]
      |
    [WOOD]-[WOOD]                   [WOOD] ← disconnected!
      |
    [LEAF]

Flood fill from SEED:
- Finds: SEED, 1 WOOD (connected via vertical path)
- Doesn't find: rightmost WOOD (no path to seed)
- Disconnected WOOD loses organism_id → falls as particle
```

### Example 3: Leaf Snaps Off
```
Tree with leaves:              Leaf breaks connection:
    [SEED]                         [SEED]
      |                              |
    [WOOD]                         [WOOD]
    /    \                         /
 [LEAF]  [LEAF]                [LEAF]    [LEAF] ← falls!

Right LEAF disconnected → organism_id=0 → becomes particle
```

## Next Steps

1. **Implement basic version** - flood fill + velocity sync
2. **Test with existing trees** - does it prevent sagging?
3. **Tune performance** - cache flood fill? Only rebuild on change?
4. **Add rotation** (later) - track angular velocity for whole structure
5. **Add fracture threshold** (later) - break connections under stress

## Open Questions

1. Should flood fill check 4 neighbors (cardinal) or 8 (with diagonals)?
2. How often to run flood fill? Every frame? Only on damage/growth?
3. Should bones still exist, or does this replace them?
4. What about multi-material organisms (ROOT + WOOD + LEAF)?

---

*Ready to implement when you are!*

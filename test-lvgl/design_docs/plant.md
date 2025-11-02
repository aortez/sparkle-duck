# Tree Organism Design

## Overview

Trees are living organisms in the WorldB physics simulation that grow from seeds, consume resources, and interact with the material physics system. This document defines the design for tree-based artificial life in the simulation.

## Design

### Core Design Decisions

#### Update Timing
- Trees update every physics timestep alongside regular material physics.
   1. Input data is gathered from the environment into each cell.
   2. Each cell's logic code runs.
   3. The tree's logic code runs, possibly initiating or terminating an existing action.
   4. Actions take a certain amount of time to complete.  Maybe have a way to track the current action.

#### Resource Systems

**Light**
- cast from top of world down
- Blocked by opaque materials
- LEAF cells collect light based on exposure
- Light drives photosynthesis energy production

**Water**
- Absorbed from adjacent WATER cells
- Can extract humidity from AIR cells (lower rate)
- Transported through tree network via capillary action
- Required for all growth and photosynthesis

**Nutrients**
- Stored in DIRT cells as a nutrient level [0.0-1.0]
- ROOT cells extract nutrients, depleting the soil
- Nutrients regenerate slowly over time (recovery)
- Nutrient levels will be stored as metadata attached to DIRT cells
- Nutrient data persists with the cell through movement and interactions
- Careful analysis of movement systems will need to be done to ensure nutrients
transfer correctly.

**Energy**
- Internal resource not visible in physics simulation
- Produced via photosynthesis equation
- Consumed by growth, maintenance, and reproduction
- Stored locally in each tree cell

#### Material Integration

**New Materials**
- SEED: Dense material, grows into tree
  - density: 8.0 (sinks in water)
  - elasticity: 0.2 (low bounce)
  - cohesion: 0.9 (stays together)
  - adhesion: 0.3 (moderate)
  - com_mass_constant: 2.5
  - is_rigid: true (like WOOD/METAL)
  - color: 0x8B4513 (saddle brown)
- ROOT: Underground tree tissue for nutrient extraction
  - density: 1.2 (denser than WOOD)
  - elasticity: 0.3 (low bounce)
  - cohesion: 0.8 (forms networks)
  - adhesion: 0.6 (grips soil)
  - com_mass_constant: 4.0
  - is_rigid: false (can bend/compress)
  - color: 0x654321 (dark brown)

**Tree Materials**
- SEED - Turns into ROOT.
- WOOD - Structural.
- ROOT - absorbs nutrients from adjacent cells and provides structure.
- LEAF - absorbs nutrients from air and light.
- All tree materials subject to physics.
- Tree cells have special "organism_id" marking ownership.

### Growth Mechanics

#### Atomic Operations
- Growth replaces target cell atomically (no intermediate empty state)
- Prevents cascading physics effects during growth
- Original material is "consumed" by the growing tree

#### Energy Costs
```
Growth Cost = Base Cost + (Target Density × Displacement Factor) + Depth Penalty
```

#### Underground Growth
- ROOT growth temporarily "locks" surrounding DIRT (how to fit into current architecture?)
- Prevents soil collapse during growth operation
- Creates stable tunnels for root networks
- Higher energy cost than above-ground growth

## Architecture

### Nursery

```cpp
/// Manages lifecycles of plants in the sim.
class Nursery {
public:
    // Lifecycle
    void update(WorldB& world, double deltaTime);
    TreeId plantSeed(const WorldB& world, uint32_t x, uint32_t y);
    void removeTree(TreeId id);

    // Resource management (Phase 3)
    void updateLightMap(const WorldB& world);
    void processPhotosynthesis();
    void distributeResources();

    // Growth (Phase 2)
    void attemptGrowth(TreeId id, WorldB& world);

    // Accessors
    const std::unordered_map<TreeId, Tree>& getTrees() const { return trees_; }

private:
    std::unordered_map<TreeId, Tree> trees_;
    std::unordered_map<Vector2i, TreeId> cell_to_tree_;  // Track which cells belong to which tree.
    std::vector<std::vector<float>> light_map_;
    uint32_t next_tree_id_ = 1;
};
```

### Tree Structure

```cpp
// TreeTypes.h
using TreeId = uint32_t;

struct TreeCell {
    Vector2i position;
    MaterialType type;  // SEED, WOOD, LEAF, or ROOT
    double energy = 0.0;
    double water = 0.0;
};

class Tree {
public:
    TreeId id;
    std::vector<TreeCell> cells;
    uint32_t age = 0;  // timesteps since planting

    // Growth parameters (for future phases)
    uint32_t growth_interval = 100;  // Timesteps between growth attempts
    double growth_energy_threshold = 10.0;

    // Resource pools (distributed across cells) - Phase 3
    double totalEnergy() const;
    double totalWater() const;
};
```

### Integration with WorldB

```cpp
class WorldB {
    // Existing members...
    std::unique_ptr<TreeManager> tree_manager_;

    // In constructor:
    tree_manager_ = std::make_unique<TreeManager>();

    // In advanceTime():
    if (tree_manager_) {
        tree_manager_->update(*this, scaledDeltaTime);
    }

    // Accessor:
    TreeManager* getTreeManager() { return tree_manager_.get(); }
    const TreeManager* getTreeManager() const { return tree_manager_.get(); }
};
```

### Material Metadata Design

CellB uses a variant-of-pointers pattern for material-specific data:
- DIRT cells: `DirtMetadata` with nutrients and water content
- AIR cells: Optional humidity data
- Tree cells: Organism ID ownership tracking
- Minimal memory overhead (8 bytes per cell)
- Only cells needing metadata pay allocation cost

## Tree Lifecycle

### Stage 1: Seed (Timesteps 0-100)
- Single SEED cell planted by user or dropped by mature tree
- Absorbs water from environment
- Checks for suitable growth conditions
- Consumes seed energy reserves

### Stage 2: Germination (Timesteps 100-200)
- SEED converts to WOOD (first growth)
- Immediately attempts to grow one ROOT downward
- Begins establishing resource networks
- Very vulnerable to resource shortage

### Stage 3: Sapling (Timesteps 200-1000)
- Rapid growth phase
- Prioritizes ROOT growth for stability and nutrients
- Begins growing LEAF cells for energy production
- Establishes basic tree structure

### Stage 4: Mature (Timesteps 1000+)
- Balanced growth based on resource availability
- Can produce new SEEDs when energy surplus exists
- Maintains existing structure
- Competes with nearby trees for resources

### Stage 5: Decline (Variable)
- Triggered by sustained resource shortage
- Individual cells begin dying (convert to DIRT)
- Can recover if conditions improve
- Complete death returns all material to environment

## Resource Equations

### Photosynthesis
```
Energy Production =
    (Light Intensity × Light Efficiency × LEAF Count) +
    (Water Available × Water Efficiency) +
    (Nutrients × Nutrient Efficiency) -
    (Maintenance Cost × Total Cells)

Where:
- Light Efficiency = 0.8 for LEAF, 0.1 for WOOD, 0.0 for ROOT
- Water Efficiency = 0.3
- Nutrient Efficiency = 0.4
- Maintenance Cost = 0.1 per cell per timestep
```

### Growth Energy Requirements
```
Growth Cost = Base + (Material Factors) + (Environmental Factors)

Base Costs:
- WOOD growth: 10 energy
- LEAF growth: 8 energy  
- ROOT growth: 12 energy

Material Factors:
- AIR displacement: ×1.0
- WATER displacement: ×1.5
- DIRT displacement: ×2.0
- Cannot displace: WALL, METAL, other trees

Environmental Factors:
- Depth penalty: +1 energy per cell below surface
- Distance from trunk: +0.5 energy per cell
```

## Growth Patterns

### Growth Priority System
1. **Survival Growth**: ROOT cells if nutrients < 20%
2. **Energy Growth**: LEAF cells if energy production negative
3. **Structural Growth**: WOOD cells for height/spread
4. **Reproductive Growth**: SEED production if surplus energy

### Directional Preferences
- **LEAF**: Grows toward highest light intensity
- **ROOT**: Grows toward highest nutrient concentration
- **WOOD**: Grows upward (against gravity) and outward

### Growth Constraints
- Must maintain connection to existing tree network
- Cannot grow through WALL or METAL
- Limited by available energy
- Respects material physics (no floating trees)

## Implementation Plan

### Phase 1: Foundation (Focus: Make seeds visible and trackable)
**Goal**: Plant SEED cells and see them render in the world. Seeds behave as normal materials until germination.

1. **Add SEED and ROOT to MaterialType enum**
   - Update MaterialType.h to include SEED and ROOT
   - Define material properties in MaterialType.cpp (see Material Integration section)
   - Add rendering colors in CellB::drawNormal() and drawDebug()

2. **Update MaterialPicker UI**
   - Modify MaterialPicker to include SEED in the 4×2 grid
   - ROOT won't be in picker (only created by tree growth)

3. **Create Tree data structures**
   - Create TreeTypes.h with TreeId typedef, TreeCell struct, and Tree class
   - TreeCell tracks position, material type, and resource levels
   - Tree class holds collection of cells and growth parameters

4. **Create TreeManager class**
   - Basic lifecycle management: plantSeed(), removeTree(), update()
   - Track trees and cell-to-tree mapping
   - Detect when user places SEED material and create Tree entity

5. **Integrate TreeManager into WorldB**
   - Add tree_manager_ member to WorldB
   - Initialize in constructor
   - Call update() in advanceTime()
   - Add accessor methods

6. **Testing**
   - Unit test: Verify SEED material properties
   - Visual test: Plant seeds, verify correct rendering
   - Physics test: Seeds fall with gravity, interact with other materials

### Phase 2: Growth System (Focus: Seeds can grow into trees)
1. **SEED → WOOD conversion** (basic germination)
2. **Add growth mechanics with atomic operations**
3. **Create growth pattern algorithms**
4. **Handle underground ROOT growth**
5. **Add organism_id to CellB for tree ownership**

### Phase 3: Resource Economy (Focus: Trees consume and produce resources)
1. **Implement light map calculation**
2. **Add photosynthesis energy production**
3. **Create water absorption from WATER/AIR**
4. **Implement nutrient extraction from DIRT**
5. **Add resource distribution through tree network**

### Phase 4: Advanced Features (Focus: Ecosystem dynamics)
1. **Multiple tree tracking and competition**
2. **Seed production and dispersal**
3. **Tree death and decomposition**
4. **Visual differentiation (tree cells slightly different color)**
5. **Performance optimization**

## Testing Strategy

### Unit Tests
- TreeManager operations (plant, remove, update)
- Resource calculations (photosynthesis, growth costs)
- Growth pattern algorithms
- Tree network connectivity

### Visual Tests
- Single tree growth over time
- Multiple tree competition
- Resource depletion and regeneration
- Tree death and decomposition

## Future Enhancements

### Genetic Variation
- Trees have variable growth parameters
- Mutation on reproduction
- Natural selection for successful traits

### Environmental Adaptation
- Trees adapt growth patterns to environment
- Drought resistance traits
- Shade tolerance variations

### Ecosystem Interactions
- Decomposer organisms
- Nutrient cycling
- Symbiotic relationships
- Seasonal variations

## Open Questions

1. **Growth Timing**: Should trees attempt growth every N timesteps or based on energy thresholds?
2. **Resource Visualization**: How to show nutrient depletion in DIRT cells? (Note: Nutrient data will be stored as metadata - see Resource Systems section)
3. **Seed Dispersal**: Physics-based (falling) or random placement?
4. **Performance Limits**: Maximum number of trees before slowdown?
5. **User Interaction**: Can users prune/harvest trees?

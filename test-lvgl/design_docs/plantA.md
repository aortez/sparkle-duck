# Tree Organism Design

## Overview

Trees are living organisms in the WorldB physics simulation that grow from seeds, consume resources, and interact with the material physics system. This document defines the complete design for tree-based artificial life in the simulation.

## Phase 0: Design Refinement

### Core Design Decisions

#### Update Timing
- Trees update every physics timestep alongside regular material physics
- Trees can choose to "act" or "wait" based on internal energy and growth timers
- No multi-timestep actions - all operations complete immediately
- Growth attempts happen every N timesteps (configurable per tree)

#### Resource Systems

**Light**
- Gradient from top of world (intensity 1.0) to bottom (intensity 0.0)
- Blocked by opaque materials (WALL, METAL have opacity 1.0)
- LEAF cells collect light based on exposure
- Light drives photosynthesis energy production

**Water**
- Absorbed from adjacent WATER cells
- Can extract humidity from AIR cells (lower rate)
- Transported through tree network via capillary action
- Required for all growth and photosynthesis

**Nutrients**
- Stored in DIRT cells as a depletion value [0.0-1.0]
- ROOT cells extract nutrients, depleting the soil
- Nutrients regenerate slowly over time (soil recovery)
- Different materials provide different nutrient levels

**Energy**
- Internal resource not visible in physics simulation
- Produced via photosynthesis equation
- Consumed by growth, maintenance, and reproduction
- Stored locally in each tree cell

#### Material Integration

**New Materials**
- SEED: Dense material (density 8.0), grows into tree
- ROOT: Underground tree tissue for nutrient extraction

**Tree Materials**
- SEED → WOOD (trunk/branches)
- WOOD → LEAF (photosynthesis) or ROOT (nutrients)
- All tree materials subject to normal physics
- Tree cells have special "organism_id" marking ownership

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
- ROOT growth temporarily "locks" surrounding DIRT
- Prevents soil collapse during growth operation
- Creates stable tunnels for root networks
- Higher energy cost than above-ground growth

## Architecture

### TreeManager

```cpp
class TreeManager {
public:
    // Lifecycle
    void update(WorldB& world, double deltaTime);
    TreeId plantSeed(uint32_t x, uint32_t y);
    void removeTree(TreeId id);
    
    // Resource management
    void updateLightMap(const WorldB& world);
    void processPhotosynthesis();
    void distributeResources();
    
    // Growth
    void attemptGrowth(TreeId id, WorldB& world);
    
private:
    std::unordered_map<TreeId, Tree> trees_;
    std::vector<std::vector<float>> light_map_;
    uint32_t next_tree_id_;
};
```

### Tree Structure

```cpp
struct TreeCell {
    Vector2i position;
    MaterialType type;  // WOOD, LEAF, ROOT
    double energy;
    double water;
};

class Tree {
public:
    TreeId id;
    std::vector<TreeCell> cells;
    uint32_t age;  // timesteps since planting
    uint32_t last_growth_timestep;
    
    // Growth parameters
    uint32_t growth_interval;  // Timesteps between growth attempts
    double growth_energy_threshold;
    
    // Resource pools (distributed across cells)
    double totalEnergy() const;
    double totalWater() const;
};
```

### Integration with WorldB

```cpp
class WorldB {
    // Existing members...
    std::unique_ptr<TreeManager> tree_manager_;
    
    // In advanceTime():
    if (tree_manager_) {
        tree_manager_->update(*this, scaledDeltaTime);
    }
};
```

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

### Phase 1: Foundation (Week 1)
1. Add SEED and ROOT to MaterialType enum
2. Create basic TreeManager class
3. Integrate TreeManager into WorldB
4. Implement seed planting mechanism
5. Basic germination (SEED → WOOD conversion)

### Phase 2: Growth System (Week 2)
1. Implement TreeCell and Tree structures
2. Add growth mechanics with atomic operations
3. Create growth pattern algorithms
4. Handle underground ROOT growth
5. Add organism_id to CellB for tree ownership

### Phase 3: Resource Economy (Week 3)
1. Implement light map calculation
2. Add photosynthesis energy production
3. Create water absorption from WATER/AIR
4. Implement nutrient extraction from DIRT
5. Add resource distribution through tree network

### Phase 4: Advanced Features (Week 4)
1. Multiple tree tracking and competition
2. Seed production and dispersal
3. Tree death and decomposition
4. Visual differentiation (tree cells slightly different color)
5. Performance optimization

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

### Performance Tests
- 100+ trees simultaneous simulation
- Light map calculation efficiency
- Resource distribution scaling

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
2. **Resource Visualization**: How to show nutrient depletion in DIRT cells?
3. **Seed Dispersal**: Physics-based (falling) or random placement?
4. **Performance Limits**: Maximum number of trees before slowdown?
5. **User Interaction**: Can users prune/harvest trees?
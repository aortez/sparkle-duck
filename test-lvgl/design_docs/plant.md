# Tree Organism Design

## Overview

Trees are living organisms in the WorldB physics simulation that grow from seeds, consume resources, and interact with the material physics system. Trees are composed of cells (SEED, WOOD, LEAF, ROOT) that participate fully in physics simulation while being coordinated by a central "brain."

## Tree-Physics Integration

**Trees are made of physics cells with organism ownership:**
- Tree cells (SEED/WOOD/LEAF/ROOT) are regular CellB objects in the world grid
- Each has an `organism_id` metadata field marking which tree owns it
- **Physics acts on tree cells normally** (gravity, pressure, collisions, forces)
- Trees can be destroyed by physics (falling metal blocks, pressure, etc.)
- Trees rely on material properties (cohesion, adhesion) for structural integrity
- TreeManager tracks which cells belong to which tree via `cell_to_tree` map

## Architecture

### File Structure

```
src/core/organisms/
├── TreeTypes.h          // TreeId, TreeCommand variants, TreeSensoryData
├── Tree.h/cpp           // Tree class
├── TreeManager.h/cpp    // Manages all trees, lifecycle
├── TreeBrain.h          // Abstract brain interface
└── brains/
    ├── RuleBasedBrain.h/cpp    // Hand-coded behavior (Phase 2)
    ├── NeuralNetBrain.h/cpp    // Neural network (future)
    └── LLMBrain.h/cpp          // Ollama integration (future)
```

### Tree Commands

Trees execute commands that take time to complete. Each command is a simple aggregate type for easy serialization.

```cpp
// TreeTypes.h

struct GrowWoodCommand {
    Vector2i target_pos;
    uint32_t execution_time = 50;  // timesteps
    double energy_cost = 10.0;
};

struct GrowLeafCommand {
    Vector2i target_pos;
    uint32_t execution_time = 30;
    double energy_cost = 8.0;
};

struct GrowRootCommand {
    Vector2i target_pos;
    uint32_t execution_time = 60;
    double energy_cost = 12.0;
};

struct ReinforceCellCommand {
    Vector2i position;
    uint32_t execution_time = 30;
    double energy_cost = 5.0;
};

struct ProduceSeedCommand {
    Vector2i position;
    uint32_t execution_time = 100;
    double energy_cost = 50.0;
};

struct WaitCommand {
    uint32_t duration = 10;
};

using TreeCommand = std::variant<
    GrowWoodCommand,
    GrowLeafCommand,
    GrowRootCommand,
    ReinforceCellCommand,
    ProduceSeedCommand,
    WaitCommand
>;
```

### Tree Class

```cpp
enum class GrowthStage {
    SEED,
    GERMINATION,
    SAPLING,
    MATURE,
    DECLINE
};

class Tree {
public:
    TreeId getId() const { return id_; }
    uint32_t getAge() const { return age_; }

    void update(WorldB& world, double deltaTime);

    // Resources (aggregated from cells)
    double getTotalEnergy() const { return total_energy_; }
    double getTotalWater() const { return total_water_; }

    // Cell tracking
    const std::unordered_set<Vector2i>& getCells() const { return cells_; }
    void addCell(Vector2i pos);
    void removeCell(Vector2i pos);

private:
    TreeId id_;
    std::unique_ptr<TreeBrain> brain_;

    // Cells owned by this tree (positions in world grid)
    std::unordered_set<Vector2i> cells_;

    // Organism state
    uint32_t age_ = 0;
    GrowthStage stage_ = GrowthStage::SEED;

    // Aggregated resources (computed from world cells)
    double total_energy_ = 0;
    double total_water_ = 0;

    // Command execution
    std::optional<TreeCommand> current_command_;
    uint32_t steps_remaining_ = 0;

    void executeCommand(WorldB& world);
    void decideNextAction(const WorldB& world);
    void updateResources(const WorldB& world);
};
```

### Tree Brain Interface

Trees have pluggable brains that make growth decisions. Brain state/memory lives in brain implementations.

```cpp
// TreeBrain.h

struct TreeSensoryData {
    // Fixed-size neural grid (scale-invariant)
    static constexpr int GRID_SIZE = 15;
    static constexpr int NUM_MATERIALS = 8;

    // Material distribution histograms for each neural cell
    // Small trees: one-hot encoding [0,0,0,1,0,0,0,0]
    // Large trees: distributions [0.4,0.1,0,0.3,0,0,0.2,0]
    std::array<std::array<std::array<double, NUM_MATERIALS>, GRID_SIZE>, GRID_SIZE>
        material_histograms;

    // Metadata about mapping
    int actual_width;       // Real bounding box size
    int actual_height;
    double scale_factor;    // Real cells per neural cell
    Vector2i world_offset;  // Top-left corner in world coords

    // Internal state
    uint32_t age;
    GrowthStage stage;
    double total_energy;
    double total_water;
    int root_count;
    int leaf_count;
    int wood_count;
};

class TreeBrain {
public:
    virtual ~TreeBrain() = default;
    virtual TreeCommand decide(const TreeSensoryData& sensory) = 0;
};
```

### Scale-Invariant Sensory System (✅ IMPLEMENTED)

Trees use a fixed 15×15 neural grid regardless of actual tree size:

**Small trees (≤15×15 cells)**:
- Fixed 15×15 world-cell viewing window centered on seed position
- 1:1 mapping (each neural cell = one world cell)
- Histograms are one-hot: [0,0,0,1,0,0,0,0,0] for pure materials
- Viewing window follows seed as it moves (physics-aware)

**Large trees (>15×15 cells)**:
- Bounding box + 1-cell padding, downsampled to 15×15
- scale_factor > 1.0, each neural cell aggregates multiple world cells
- Histograms show distributions: [0.4 WOOD, 0.3 LEAF, 0.2 AIR, 0.1 DIRT]

**Implementation Notes**:
- Seed position tracked via Tree.seed_position (updated on transfers)
- Material histograms populated by sampling world grid (Tree.cpp:275-299)
- Visualization in UI via NeuralGridRenderer (50/50 split with world view)

```cpp
TreeSensoryData Tree::gatherSensoryData(const WorldB& world) {
    TreeSensoryData data;

    // Calculate bounding box + 1 cell padding
    auto bbox = calculateBoundingBox();
    bbox.expand(1);

    data.actual_width = bbox.width;
    data.actual_height = bbox.height;
    data.world_offset = bbox.top_left;
    data.scale_factor = std::max(
        (double)bbox.width / TreeSensoryData::GRID_SIZE,
        (double)bbox.height / TreeSensoryData::GRID_SIZE
    );

    // Compute histogram for each neural grid cell
    for (int ny = 0; ny < 15; ny++) {
        for (int nx = 0; nx < 15; nx++) {
            // Map neural coords to world region
            int wx_start = bbox.x + (int)(nx * data.scale_factor);
            int wy_start = bbox.y + (int)(ny * data.scale_factor);
            int wx_end = bbox.x + (int)((nx + 1) * data.scale_factor);
            int wy_end = bbox.y + (int)((ny + 1) * data.scale_factor);

            // Count materials in region and normalize
            data.material_histograms[ny][nx] =
                computeHistogram(world, wx_start, wy_start, wx_end, wy_end);
        }
    }

    return data;
}
```

### TreeManager

```cpp
class TreeManager {
public:
    void update(WorldB& world, double deltaTime);
    TreeId plantSeed(WorldB& world, uint32_t x, uint32_t y);
    void removeTree(TreeId id);
    void notifyTransfers(const std::vector<OrganismTransfer>& transfers);  // ✅ IMPLEMENTED

    const std::unordered_map<TreeId, Tree>& getTrees() const { return trees_; }

private:
    std::unordered_map<TreeId, Tree> trees_;
    std::unordered_map<Vector2i, TreeId> cell_to_tree_;
    uint32_t next_tree_id_ = 1;
};
```

**Organism Tracking (✅ IMPLEMENTED)**:
- Physics transfers automatically preserve organism_id (Cell.cpp:198-206)
- World collects OrganismTransfer events during applyTransfers()
- TreeManager::notifyTransfers() batch-updates tracking in O(transfers)
- Tree.cells and cell_to_tree_ map stay synchronized with physics
- seed_position updated when seed cell moves

### Update Flow

```cpp
void Tree::update(WorldB& world, double deltaTime) {
    age_++;

    // Execute current command (tick down timer)
    if (current_command_.has_value()) {
        if (--steps_remaining_ == 0) {
            executeCommand(world);
            current_command_.reset();
        }
    }

    // When idle, ask brain for next action
    if (!current_command_.has_value()) {
        decideNextAction(world);
    }

    // Resource updates (continuous)
    updateResources(world);
}
```

## Growth Mechanics

### Atomic Replacement

Growth replaces target cell atomically to prevent cascading physics effects:

```cpp
void Tree::executeCommand(WorldB& world) {
    std::visit([&](auto&& cmd) {
        using T = std::decay_t<decltype(cmd)>;

        if constexpr (std::is_same_v<T, GrowWoodCommand> ||
                      std::is_same_v<T, GrowLeafCommand> ||
                      std::is_same_v<T, GrowRootCommand>) {

            Cell& target = world.getCell(cmd.target_pos);

            // TODO: Generate dynamic pressure from displaced material
            // This would push adjacent materials outward realistically

            // Simple replacement (material disappears)
            MaterialType new_type = /* extract from command type */;
            target.setMaterialType(new_type);
            target.setFillRatio(1.0);
            target.setOrganismId(id_);

            cells_.insert(cmd.target_pos);
            total_energy_ -= cmd.energy_cost;
        }
    }, *current_command_);
}
```

### Growth Constraints
- Must be adjacent to existing tree cells
- Cannot grow through WALL or METAL
- Cannot grow into other trees
- Limited by available energy

## Materials

### New Material Types

**SEED** - Dense material, grows into tree
- density: 8.0 (sinks in water)
- elasticity: 0.2
- cohesion: 0.9 (stays together)
- adhesion: 0.3
- is_rigid: true
- color: 0x8B4513 (saddle brown)

**ROOT** - Underground tree tissue
- density: 1.2
- elasticity: 0.3
- cohesion: 0.8 (forms networks)
- adhesion: 0.6 (grips soil)
- is_rigid: false (can bend)
- color: 0x654321 (dark brown)

**Tree Materials**:
- SEED - Converts to WOOD during germination
- WOOD - Structural (already exists)
- ROOT - Nutrient extraction, underground structure
- LEAF - Energy production (already exists)
- All subject to normal physics
- Marked with organism_id

## Resource Systems

**Air**
- Absorbed by leaves and at a lower rate, by roots in dirt or air.
- Required for photosynthesis.

**Light**
- Cast from top of world downward
- Blocked by opaque materials
- LEAF cells collect light based on exposure
- Drives photosynthesis

**Water**
- Absorbed from adjacent WATER cells (very high rate)
- Can extract from DIRT (low rate)
- Can extract from AIR (very low rate)
- Required for growth and photosynthesis

**Nutrients**
- Stored in DIRT cells as metadata [0.0-1.0]
- ROOT cells extract nutrients
- Regenerate slowly over time

**Energy**
- Internal tree resource
- Produced via photosynthesis
- Consumed by growth and maintenance
- Tracked per tree, distributed across cells

### Photosynthesis (Phase 3)
```
Energy Production =
    (Light × Light_Efficiency × LEAF_Count) +
    (Water × Water_Efficiency) +
    (Nutrients × Nutrient_Efficiency) -
    (Maintenance_Cost × Total_Cells)
```

### Growth Costs
```
Base Costs:
- WOOD: 10 energy, 50 timesteps
- LEAF: 8 energy, 30 timesteps
- ROOT: 12 energy, 60 timesteps

Material displacement multipliers (future):
- AIR: ×1.0
- WATER: ×1.5
- DIRT: ×2.0
```

## Tree Lifecycle Stages

**SEED** (0-100 timesteps)
- Single cell, absorbs water
- Waits for germination conditions

**GERMINATION** (100-200 timesteps)
- SEED → WOOD conversion
- Grows first ROOT downward
- Very vulnerable

**SAPLING** (200-1000 timesteps)
- Rapid growth
- Prioritizes ROOTs and LEAFs
- Establishes structure

**MATURE** (1000+ timesteps)
- Balanced growth
- Can produce SEEDs
- Competes with other trees

**DECLINE** (variable)
- Resource shortage triggers
- Cells die (convert to DIRT)
- Can recover if conditions improve

## Growth Patterns (Brain Implementations)

### Priority System (Example for RuleBasedBrain)
1. Survival: ROOT cells if nutrients < 20%
2. Energy: LEAF cells if energy production negative
3. Structure: WOOD cells for height/spread
4. Reproduction: SEED production if surplus energy

### Directional Preferences
- LEAF: Grows toward light and away from wood
- ROOT: Grows toward dirt and away from wood
- WOOD: Grows upward and away from other wood

## Implementation Plan

### Phase 1: Foundation + Neural Grid Visualization
**Goal**: SEED material, tree organisms, and visual debugging of tree perception

**Status: ✅ COMPLETE**

Completed:
- ✅ Add SEED to MaterialType enum (alphabetized)
- ✅ Define SEED material properties (density: 3.5, rigid, cohesive)
- ✅ Add SEED rendering color (0x8B4313 - saddle brown)
- ✅ Create SeedAdd API command
- ✅ Add UI button for placing seeds
- ✅ Create TreeCommands.h, TreeSensoryData.h/cpp (TreeTypes.h deleted, split into focused files)
- ✅ Create TreeBrain.h abstract interface
- ✅ Create Tree.h/cpp with command execution system
- ✅ Create TreeManager.h/cpp for lifecycle management
- ✅ Add organism_id field to Cell class
- ✅ Integrate TreeManager into World class
- ✅ Update CMakeLists.txt with organism source files
- ✅ Add std::hash<Vector2i> specialization for unordered containers
- ✅ Seeds fall with gravity and participate in full physics simulation
- ✅ NeuralGridRenderer for 15×15 tree vision display (UI side-by-side layout)
- ✅ Efficient organism transfer tracking (O(transfers) batch updates)
- ✅ organism_id automatically transferred with material movements
- ✅ TreeSensoryData serialization (JSON + zpp_bits)
- ✅ seed_position tracking for neural grid centering
- ✅ Material histogram population from world state

Deferred to Phase 3+:
- ❌ Material picker UI (doesn't exist yet - seeds placeable via SeedAdd command)

### Phase 2: Growth System
**Goal**: Intelligent germination and balanced growth with resource constraints

**Status: ✅ MOSTLY COMPLETE**

Completed:
- ✅ Add ROOT material type (density: 1.2, grips soil with adhesion 0.6, can bend)
- ✅ ROOT rendering (color: 0x654321 dark brown, emoji: 🌿)
- ✅ Continuous time system (deltaTime-based, all timing in seconds)
- ✅ TreeCommandProcessor for validated command execution
- ✅ Contact-based germination (detect dirt → observe 2s → grow ROOT 2s → grow WOOD 3s)
- ✅ SEED stays permanent as tree core (not replaced during germination)
- ✅ Adjacency validation (allow AIR/DIRT/SAND, block WALL/METAL/WATER)
- ✅ Energy validation (commands rejected if insufficient energy)
- ✅ RuleBasedBrain with intelligent growth decisions
- ✅ Balanced growth system (analyzes material_histograms, maintains target ratios)
- ✅ Water-seeking behavior (roots target 30% → 15% when water found)
- ✅ LEAF air-only restriction (leaves require AIR > 0.5, no underground foliage)
- ✅ LEAF growth from WOOD cells (cardinal directions, prefers outward expansion)
- ✅ Swap physics integration (organism tracking works with material swaps)
- ✅ UI enhancements (energy display, current thought display)
- ✅ Test infrastructure (6 passing tests with emoji visualization)
- ✅ Stage transitions (SEED → GERMINATION → SAPLING)

Limitations:
- Growth happens from seed position only (not from tree edges) - trees grow as blobs
- No energy regeneration (trees deplete and stop growing)
- SAPLING continues indefinitely (no MATURE transition)
- No death/decomposition when energy depleted

Next steps for Phase 2 completion:
- Fix growth topology (extend from edges, not seed center)
- Add MATURE stage transition (based on size or age)
- Basic energy regeneration (LEAFs produce small amounts over time)

### Phase 3: Resource Economy
1. Light map calculation
2. Photosynthesis implementation
3. Water absorption
4. Nutrient extraction from DIRT
5. Testing: Trees survive/die based on resources

### Phase 4: Advanced Features
1. Multiple tree competition
2. Seed production and dispersal
3. Tree death/decomposition
4. Visual differentiation
5. Performance optimization

### Phase 5: AI Brains (Future)

#### Neural Network Brain

**Architecture**:
- Input layer: ~1806 neurons
  - 15×15×8 = 1800 for material histograms
  - 6 for internal state (energy, water, counts, scale_factor)
- Hidden layers: 32-64 neurons (1-2 layers) - is this big enough?
- Output layer: 676 neurons
  - 15×15×3 = 675 for grow commands (WOOD/LEAF/ROOT at each position)
  - 1 for WAIT command

**Action Space**:
Output neurons map directly to neural grid positions:
- Neurons [0-224]: GROW_WOOD at each of 15×15 positions
- Neurons [225-449]: GROW_LEAF at each of 15×15 positions
- Neurons [450-674]: GROW_ROOT at each of 15×15 positions
- Neuron [675]: WAIT

Brain selects action with highest activation (argmax), maps neural coordinates back to world coordinates using scale_factor.

**Genetic Algorithm Evolution**:
- Population of trees with random initial weights
- Fitness based on: survival time, growth, resource efficiency
- Selection, crossover, mutation of network weights
- Multiple generations evolve successful growth strategies

**Benefits of histogram-based input**:
- Fixed network size regardless of tree size
- Network learns to interpret "blur" (distributions vs one-hot)
- Small trees get crisp signals, large trees get aggregated view
- Smooth transition as tree grows, no discontinuities

#### LLM Brain

Uses Ollama for high-level strategic decision making (see design_docs/ai-integration-ideas.md).

#### Brain Comparison Tools

- Visualize evolved network weights
- Compare fitness across different architectures
- Export successful strategies for analysis

## Integration with World

```cpp
class World {
    std::unique_ptr<TreeManager> tree_manager_;

    // In constructor:
    tree_manager_ = std::make_unique<TreeManager>();

    // In advanceTime():
    if (tree_manager_) {
        tree_manager_->update(*this, scaledDeltaTime);
    }

    TreeManager* getTreeManager() { return tree_manager_.get(); }
};
```

## Testing Strategy

### Unit Tests
- TreeManager operations
- Resource calculations
- Command execution
- Cell ownership tracking

### Visual Tests
- Single tree growth
- Multiple tree competition
- Resource depletion
- Physics destruction (falling blocks crushing trees)

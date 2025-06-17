# WorldB Development Plan
## Advanced Pure-Material Physics System

Now that runtime world switching is implemented, WorldB can be developed as a full-featured physics system independent of WorldA. This plan outlines the roadmap for expanding WorldB into a rich, interactive material simulation.

## Current State Assessment

### ✅ **Completed Foundation**
- **8 Material Types**: AIR, DIRT, WATER, WOOD, SAND, METAL, LEAF, WALL
- **Complete Material Rendering**: All 8 material types visually implemented with distinct colors and textures
- **Material Picker UI**: 4×2 grid layout for selecting any material type
- **Smart Cell Grabber**: Intelligent interaction system with floating particle preview
- **Basic Physics**: Gravity, velocity limiting, momentum conservation
- **Material Properties**: Density, elasticity, cohesion, adhesion, fluid/rigid flags
- **Rendering Pipeline**: LVGL-based cell rendering with debug visualization
- **UI Integration**: Full WorldInterface compatibility with material selection
- **Runtime Switching**: Live WorldA ↔ WorldB transitions with state preservation

### ✅ **Recently Completed**
- **Enhanced Collision System**: Material-specific collision behaviors with elastic, inelastic, fragmentation, and absorption types
- **Velocity Vector Visualization**: Fixed to start from COM position instead of cell center
- **Comprehensive Material Interaction Matrix**: METAL vs METAL, METAL vs WALL, WOOD vs rigid materials, etc.
- **Restitution Coefficient System**: Material-specific bounce factors based on elasticity properties
- **Two-Body Elastic Collision Physics**: Proper momentum and energy conservation

### 🔄 **Current Work In Progress**
- **Universal Floating Particle System**: Converting all materials to interactive floating particles

### 🔧 **Current Limitations**
- **Underutilized Material Properties**: Physics doesn't fully use cohesion/adhesion (elasticity now utilized)
- **Limited Material Interactions**: No material mixing or chemical reactions (collision detection now comprehensive)

## Phase 1: Advanced Debug Visualization ⭐ **HIGH PRIORITY**

### 1.1 Enhanced Debug Information Display

**Goal**: Rich debugging information for development and education

**Features**:
- **Material Properties Overlay**: Show density, elasticity values
- **Physics State Indicators**: Velocity vectors, COM position, pressure
- **Transfer Visualization**: Arrows showing material flow direction
- **Material Interaction Highlights**: Show cohesion/adhesion effects
- **Performance Metrics**: Frame rate, physics calculation times
- **Material Distribution**: Real-time histograms of material amounts

**Implementation**:
```cpp
class DebugRenderer {
public:
    void renderVelocityVectors(const WorldB& world, lv_obj_t* canvas);
    void renderMaterialProperties(const CellB& cell, lv_obj_t* canvas);
    void renderTransferFlow(const WorldB& world, lv_obj_t* canvas);
    void renderPhysicsMetrics(const WorldB& world, lv_obj_t* canvas);
};
```

## Phase 2: Enhanced Physics and Material Interactions

### 2.1 Material-Specific Physics Behaviors ⭐ **HIGH PRIORITY**

**Goal**: Each material type has unique physics properties

**Enhanced Physics**:
```cpp
class MaterialPhysics {
public:
    // Material-specific transfer behaviors
    bool canMaterialsTransfer(MaterialType from, MaterialType to);
    double calculateTransferRate(MaterialType type, double pressure);
    Vector2d calculateMaterialForces(MaterialType type, const CellB& cell);
    
    // Material interaction effects
    void applyMaterialCohesion(CellB& cell, MaterialType type);
    void applyMaterialAdhesion(CellB& cell, const CellB& neighbor);
    void handleMaterialCollision(CellB& cell1, CellB& cell2);
};
```

**Material Behaviors**:
- **WATER**: Flows easily, high transfer rates, low cohesion
- **SAND**: Granular flow, medium transfer rates, angle of repose
- **DIRT**: Clumpy behavior, variable cohesion based on "moisture"
- **WOOD**: Rigid, compression-only, high elasticity
- **METAL**: Very rigid, high density, excellent force transmission
- **LEAF**: Light, low density, high air resistance
- **WALL**: Immobile, infinite resistance

## Implementation Priority

### **Phase 1**
1. ⭐ Advanced debug visualization and information overlay
2. 🔄 ~~Enhanced collision system~~ **IN PROGRESS** (external development)

### **Phase 2**
3. ⭐ Material-specific physics behaviors (cohesion, adhesion, transfer rates)
4. Design Pressure. 
5. Implement Pressure.
5. Material interaction system (mixing, reactions)

## Technical Architecture

### **Modular Design**
- **MaterialRenderer**: Handles all material visualization
- **MaterialPicker**: UI for material selection  
- **CellGrabber**: Interactive cell manipulation
- **MaterialPhysics**: Material-specific physics behaviors

### **Integration Points**
- **WorldInterface**: Extended with material-specific methods
- **SimulatorUI**: Enhanced with new tool panels
- **CellB**: Extended with material interaction methods
- **MaterialType**: Potentially extensible for custom materials

This plan transforms WorldB from a basic pure-material system into a rich, interactive physics playground that takes full advantage of its clean material-based architecture!
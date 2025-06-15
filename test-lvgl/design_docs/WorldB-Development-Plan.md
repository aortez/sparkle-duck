# WorldB Development Plan
## Advanced Pure-Material Physics System

Now that runtime world switching is implemented, WorldB can be developed as a full-featured physics system independent of WorldA. This plan outlines the roadmap for expanding WorldB into a rich, interactive material simulation.

## Current State Assessment

### ✅ **Completed Foundation**
- **8 Material Types**: AIR, DIRT, WATER, WOOD, SAND, METAL, LEAF, WALL
- **Basic Physics**: Gravity, velocity limiting, momentum conservation
- **Material Properties**: Density, elasticity, cohesion, adhesion, fluid/rigid flags
- **Rendering Pipeline**: LVGL-based cell rendering with debug visualization
- **UI Integration**: Full WorldInterface compatibility
- **Runtime Switching**: Live WorldA ↔ WorldB transitions with state preservation

### 🔧 **Current Limitations**
- **Limited Material Rendering**: Only DIRT and WATER visually implemented
- **Basic Material Addition**: Only addDirtAtPixel() and addWaterAtPixel()
- **Simple User Interaction**: No material selection or advanced tools
- **Underutilized Material Properties**: Physics doesn't fully use cohesion/adhesion
- **Basic Transfer System**: Simple momentum conservation, no material-specific behaviors

## Phase 1: Complete Material Rendering System

### 1.1 Enhanced Material Visualization ⭐ **HIGH PRIORITY**

**Goal**: Render all 8 material types with distinct visual characteristics

**Implementation**:
```cpp
// CellB.cpp enhancements
class CellB {
    // Material-specific rendering methods
    void renderMaterial(MaterialType type, double fillRatio, lv_obj_t* canvas);
    lv_color_t getMaterialColor(MaterialType type, bool debug_mode);
    uint8_t calculateMaterialOpacity(MaterialType type, double fillRatio);
    
    // Enhanced debug visualization
    void renderMaterialBorder(MaterialType type, lv_obj_t* canvas);
    void renderMaterialTexture(MaterialType type, double fillRatio, lv_obj_t* canvas);
};
```

**Material Visual Design**:
- **DIRT**: Saddle brown with granular texture
- **WATER**: Animated blue with flow patterns  
- **WOOD**: Dark brown with wood grain texture
- **SAND**: Light brown with particle appearance
- **METAL**: Silver/grey with metallic shine
- **LEAF**: Green with organic texture
- **WALL**: Dark grey with brick/stone pattern
- **AIR**: Transparent (current black background)

**Features**:
- Material-specific textures using LVGL patterns
- Fill-ratio based transparency for realistic density
- Debug mode: material borders, property indicators
- Animation effects for fluids (water ripples, sand flow)

### 1.2 Advanced Debug Visualization

**Goal**: Rich debugging information for development and education

**Features**:
- **Material Properties Overlay**: Show density, elasticity values
- **Physics State Indicators**: Velocity vectors, COM position, pressure
- **Transfer Visualization**: Arrows showing material flow direction
- **Material Interaction Highlights**: Show cohesion/adhesion effects

## Phase 2: Material Selection UI System

### ✅ 2.1 Material Picker Interface **COMPLETED**

**Goal**: Allow users to select and place any of the 8 material types

**✅ Implemented Features**:
- **MaterialPicker Class**: Complete 4×2 grid layout with LVGL integration
- **Material Selection UI**: All 8 material types displayed with color-coded buttons
- **WorldInterface Extension**: Added universal material addition methods
- **Cross-World Compatibility**: Material mapping for WorldA, direct support for WorldB
- **UI Integration**: MaterialPicker positioned in SimulatorUI below WorldType switch

**✅ Implementation**:
```cpp
class MaterialPicker {
    static constexpr int GRID_ROWS = 4;
    static constexpr int GRID_COLS = 2;
    static constexpr int TOTAL_MATERIALS = 8;
    // 4×2 grid layout with all 8 material types visible
    // Integrated event handling and material selection state
};
```

**✅ Interface Layout**:
- **Material Grid**: 4×2 grid of material buttons with color coding
- **Visual Material Icons**: Each button shows material-specific colors
- **Compact Design**: Fits in narrow UI column without scrolling
- **Event Integration**: Click handlers for material selection

**✅ WorldInterface Integration**:
```cpp
// Extended WorldInterface for universal material addition
virtual void addMaterialAtPixel(int pixelX, int pixelY, MaterialType type, double amount = 1.0) = 0;
virtual void setSelectedMaterial(MaterialType type) = 0;
virtual MaterialType getSelectedMaterial() const = 0;
```

## Phase 2: Cell Grabber and Interaction System

### 2.2 Cell Grabber Utility ⭐ **HIGH PRIORITY** 

**Goal**: Interactive cell manipulation for advanced user control

**Implementation**:
```cpp
class CellGrabber {
public:
    void startGrabbing(int pixelX, int pixelY, MaterialType filter = MaterialType::AIR);
    void updateGrabPosition(int pixelX, int pixelY);
    void endGrabbing(int pixelX, int pixelY);
    void cancelGrabbing();
       
private:
    struct GrabbedCell {
        MaterialType material;
        double amount;
        Vector2d velocity;
        Vector2d com;
    };
    
    GrabbedCell grabbed_cell_;
};
```

**Grabber Features**:
- **Single Cell Grab**: Grab and manipulate one cell at a time
- **Grab Visualization**: Show grabbed cell with overlay
- **Affect on Particle**: Moves the particle by applying a force to it, increasing relative to its offset from the cursor.
- **Ability to "toss the particle", having it track the recent cursor movement's.

## Phase 3: Enhanced Physics and Material Interactions

### 3.1 Material-Specific Physics Behaviors

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

### 3.2 Material Interaction System

**Goal**: Materials interact with each other realistically

**Interactions**:
- **Material Mixing**: Some materials can create composite behaviors
- **Chemical Reactions**: Future expansion for material transformation
- **Thermal Conduction**: Different materials conduct heat differently
- **Electrical Conduction**: METAL conducts, others resist

## Phase 4: Advanced Features and Tools

### 4.1 World Generation Tools

**Goal**: Create complex scenarios quickly

**Tools**:
- **Terrain Generator**: Create landscapes with different materials
- **Structure Builder**: Pre-built structures (walls, containers, ramps)
- **Material Layers**: Create stratified material compositions
- **Random Fill**: Procedurally place materials with noise patterns

### 4.2 Simulation Scenarios

**Goal**: Pre-configured interesting physics demonstrations

**Scenarios**:
- **Hourglass**: SAND flowing through narrow openings
- **Dam Break**: WATER pressure simulation
- **Landslide**: DIRT/SAND slope stability
- **Material Separation**: Density-based material sorting
- **Collision Physics**: METAL ball interactions

### 4.3 Analysis and Measurement Tools

**Goal**: Quantitative analysis of simulation behavior

**Tools**:
- **Pressure Measurement**: Click to see pressure at any point
- **Velocity Measurement**: Show velocity magnitude and direction
- **Material Distribution**: Real-time histogram of material amounts
- **Energy Conservation**: Track kinetic and potential energy
- **Flow Rate Measurement**: Measure material transfer rates

## Phase 5: Performance and Optimization

### 5.1 Rendering Optimization

**Goal**: Smooth performance with complex material interactions

**Optimizations**:
- **Dirty Region Tracking**: Only redraw changed areas
- **Level of Detail**: Simplified rendering at low zoom levels
- **Material Culling**: Skip invisible material calculations
- **Batch Rendering**: Group similar materials for efficient drawing

### 5.2 Physics Optimization

**Goal**: Maintain real-time performance with large simulations

**Optimizations**:
- **Spatial Partitioning**: Optimize neighbor searches
- **Sleeping Cells**: Skip calculations for stationary cells
- **Adaptive Time Stepping**: Variable physics update rates
- **Multi-threading**: Parallel processing for independent regions

## Implementation Priority

### **Phase 1 (Immediate - Next 1-2 weeks)**
1. ⭐ Complete material rendering for all 8 types
2. ✅ ~~Material picker UI interface~~ **COMPLETED**
3. ⭐ Basic cell grabber implementation

### **Phase 2 (Short-term - 1-2 months)**
4. Enhanced physics behaviors for each material
5. Material interaction system
6. Advanced placement tools (brush, fill, shapes)

### **Phase 3 (Medium-term - 2-4 months)**  
7. Simulation scenarios and world generation
8. Analysis and measurement tools
9. Performance optimization

### **Phase 4 (Long-term - 4+ months)**
10. Advanced features (thermal, electrical simulation)
11. Custom material definition system
12. Plugin architecture for user extensions

## Technical Architecture

### **Modular Design**
- **MaterialRenderer**: Handles all material visualization
- **MaterialPicker**: UI for material selection  
- **CellGrabber**: Interactive cell manipulation
- **MaterialPhysics**: Material-specific physics behaviors
- **ScenarioManager**: Pre-built simulation setups
- **AnalysisTools**: Measurement and analysis utilities

### **Integration Points**
- **WorldInterface**: Extended with material-specific methods
- **SimulatorUI**: Enhanced with new tool panels
- **CellB**: Extended with material interaction methods
- **MaterialType**: Potentially extensible for custom materials

This plan transforms WorldB from a basic pure-material system into a rich, interactive physics playground that takes full advantage of its clean material-based architecture!
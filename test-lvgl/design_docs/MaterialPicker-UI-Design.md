# Material Picker UI Design

## Overview

The Material Picker UI provides users with an intuitive interface to select from all 8 WorldB material types for placement in the simulation. The picker uses visual consistency by reusing the cell rendering system to create recognizable material icons on selection buttons.

## Current Status

**🔄 IN DEVELOPMENT** - Currently being implemented as part of WorldB Phase 1 development.

## Design Goals

### Primary Objectives
- **Universal Compatibility**: Works with both WorldA (RulesA) and WorldB (RulesB) physics systems
- **Visual Consistency**: Material buttons use the same rendering as simulation cells
- **Intuitive Interface**: Clear material selection with visual feedback
- **Clean Architecture**: Separate MaterialPicker class with clear ownership boundaries

### User Experience Goals
- One-click material selection from 8 available types
- Clear visual indication of currently selected material
- Seamless integration with existing draw area interactions
- Consistent behavior across both physics systems

## Technical Architecture

### Class Structure

#### MaterialPicker Class
```cpp
class MaterialPicker {
public:
    MaterialPicker(lv_obj_t* parent);
    ~MaterialPicker();
    
    // UI Creation
    void createMaterialSelector();
    void createMaterialButton(MaterialType type, int gridX, int gridY);
    
    // Material Selection
    MaterialType getSelectedMaterial() const;
    void setSelectedMaterial(MaterialType type);
    
    // Event Handling
    static void onMaterialButtonClicked(lv_event_t* e);
    
private:
    lv_obj_t* parent_;
    lv_obj_t* material_grid_;
    lv_obj_t* material_buttons_[8];
    MaterialType selected_material_;
    
    // Button Graphics
    void createMaterialIcon(lv_obj_t* button, MaterialType type);
    void updateButtonHighlight(MaterialType type);
};
```

#### SimulationManager Integration
- **Material State Ownership**: SimulationManager owns the default/selected material state
- **MaterialPicker Access**: Provides getter/setter methods for material selection
- **UI Coordination**: Updates MaterialPicker when material changes externally

### Material Mapping Strategy

#### WorldB (RulesB) - Direct Mapping
All 8 materials supported directly:
- **AIR**: Clear cells (remove existing material)
- **DIRT**: Brown granular material
- **WATER**: Blue fluid material  
- **WOOD**: Dark brown solid material
- **SAND**: Light brown granular material
- **METAL**: Silver/grey solid material
- **LEAF**: Green organic material
- **WALL**: Grey immovable barrier

#### WorldA (RulesA) - Compatibility Mapping
Maps 8 materials to dirt/water/nothing system:
- **DIRT** → `addDirtAtPixel()` (full amount)
- **WATER** → `addWaterAtPixel()` (full amount)
- **SAND** → `addDirtAtPixel()` (mapped as dirt-like material)
- **WOOD** → `addDirtAtPixel()` (mapped as solid dirt)
- **METAL** → `addDirtAtPixel()` (mapped as dense dirt)
- **LEAF** → `addWaterAtPixel()` (mapped as light water)
- **WALL** → `addDirtAtPixel()` (mapped as very dense dirt)
- **AIR** → No operation (cannot remove materials in WorldA)

### UI Layout and Integration

#### Control Panel Placement
- **Location**: Below WorldType switch control in SimulatorUI
- **Layout**: 2×4 grid of material selection buttons
- **Size**: 4 buttons × 64px width + spacing = ~280px total width
- **Button Size**: 64×64px buttons with 32×32px material icons

#### Visual Design
- **Material Icons**: Mini-cell renderings using actual CellB::draw() system
- **Selected Highlight**: Border highlight around currently selected material
- **Default Selection**: DIRT material selected by default
- **Hover Effects**: Subtle visual feedback for button interactions

### WorldInterface Extensions

#### New Methods
```cpp
// Universal material addition (works with both WorldA and WorldB)
virtual void addMaterialAtPixel(int pixelX, int pixelY, MaterialType type, double amount = 1.0) = 0;

// Material selection state (for UI synchronization)
virtual void setSelectedMaterial(MaterialType type) = 0;
virtual MaterialType getSelectedMaterial() const = 0;
```

#### Implementation Strategy
- **WorldB**: Direct implementation using existing material system
- **WorldA**: Mapping layer that converts MaterialType to appropriate dirt/water calls
- **Fallback**: Graceful handling of unsupported materials

## Implementation Plan

### Phase 1: Core Infrastructure
1. **MaterialPicker Class Creation**
   - Create `MaterialPicker.h` and `MaterialPicker.cpp`
   - Implement 2×4 button grid with LVGL
   - Add material selection state management
   - Create button click event handlers

2. **Mini-Cell Rendering System**
   - Create 32×32px canvases for button icons
   - Reuse CellB rendering with scaled dummy cells
   - Ensure visual consistency with simulation
   - Handle all 8 material types
   - Is ImageButton a good solution here? https://docs.lvgl.io/master/details/widgets/imagebutton.html 

### Phase 2: WorldInterface Extension
3. **Universal Material Addition**
   - Add `addMaterialAtPixel()` method to WorldInterface
   - Implement direct support in WorldB
   - Create mapping layer for WorldA compatibility
   - Add material selection state methods

### Phase 3: UI Integration
4. **SimulatorUI Integration**
   - Add MaterialPicker to control panel layout
   - Position below WorldType switch as specified
   - Wire up material selection with draw area events
   - Update existing mouse interaction handlers

5. **SimulationManager Coordination**
   - Add selected material state to SimulationManager
   - Provide MaterialPicker with access to selection state
   - Handle material updates and UI synchronization
   - Set DIRT as default selected material

### Phase 4: Enhanced Interactions
6. **Draw Area Event Updates**
   - Replace hardcoded `addDirtAtPixel()`/`addWaterAtPixel()` calls
   - Use selected material from SimulationManager
   - Maintain existing drag and click behaviors
   - Support all 8 materials in mouse interactions

## Technical Considerations

### Performance
- **Button Rendering**: One-time icon generation, cached in button objects
- **Material Selection**: O(1) state updates with efficient UI highlighting
- **Draw Area Integration**: Minimal overhead for material type checking

### Memory Management
- **Icon Storage**: Small 32×32px canvases per button (~4KB each)
- **Button Objects**: Standard LVGL button memory usage
- **State Management**: Single MaterialType enum value in SimulationManager

### Error Handling
- **Invalid Materials**: Graceful fallback to DIRT for unknown types
- **WorldA Limitations**: Clear mapping for unsupported materials
- **UI Updates**: Robust synchronization between picker and simulation state

## Future Extensions

### Enhanced Features (Post-Phase 1)
- **Material Properties Display**: Show density, elasticity when hovering
- **Custom Material Colors**: User-configurable material appearance
- **Material Amount Control**: Slider for controlling placement density
- **Material Mixing**: Support for composite materials in WorldA

### Advanced UI Elements
- **Brush Size Control**: Variable placement area size
- **Material Info Panel**: Detailed properties of selected material
- **Quick Switch Hotkeys**: Keyboard shortcuts for material selection
- **Material History**: Recently used materials for quick access

## Success Criteria

### Functional Requirements
1. **Universal Operation**: Works identically with both WorldA and WorldB
2. **Visual Consistency**: Material buttons match simulation appearance
3. **State Management**: Selected material persists across interactions
4. **Seamless Integration**: Natural fit within existing UI layout

### User Experience Requirements
1. **Intuitive Selection**: One-click material switching
2. **Clear Feedback**: Obvious indication of selected material
3. **Responsive Interface**: Immediate visual updates on selection
4. **Consistent Behavior**: Predictable material placement results

### Technical Requirements
1. **Clean Architecture**: Separate MaterialPicker class with clear interfaces
2. **Efficient Rendering**: Minimal performance impact on simulation
3. **Robust Error Handling**: Graceful handling of edge cases
4. **Maintainable Code**: Clear separation of concerns and documentation

## Implementation Notes

### Dependencies
- **CellB Rendering System**: Required for consistent material icons
- **MaterialType Enum**: Complete implementation of all 8 material types
- **WorldInterface**: Extended with universal material addition methods
- **SimulationManager**: Enhanced with material selection state management

### Integration Points
- **SimulatorUI**: Control panel layout and event coordination
- **Draw Area Events**: Mouse interaction and material placement
- **WorldFactory**: Potential future integration for world-specific features
- **Material Properties**: Integration with existing material system

This design ensures a cohesive, user-friendly material selection interface that enhances the simulation experience while maintaining compatibility with both physics systems.
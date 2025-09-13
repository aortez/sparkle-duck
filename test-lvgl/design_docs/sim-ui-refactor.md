# SimulatorUI Refactoring with LVGLBuilder

## Overview

This document outlines a comprehensive refactoring plan for the SimulatorUI system to create reusable UI components that can be shared across multiple interfaces (main simulator, visual test framework, and future UIs).

## Current State Analysis

### SimulatorUI.cpp Issues
- **Size**: 1780 lines - too large for maintainability
- **Repetitive patterns**: Manual LVGL boilerplate repeated for every UI element
- **Hard-coded positioning**: Manual coordinate calculations scattered throughout
- **Tight coupling**: UI creation mixed with event handling and business logic

### Common LVGL Patterns Identified

1. **Slider Creation** (30+ instances):
```cpp
lv_obj_t* slider = lv_slider_create(screen_);
lv_obj_set_size(slider, CONTROL_WIDTH, 10);
lv_obj_align(slider, LV_ALIGN_TOP_LEFT, x, y);
lv_slider_set_range(slider, min, max);
lv_slider_set_value(slider, default_val, LV_ANIM_OFF);
lv_obj_add_event_cb(slider, callback, LV_EVENT_ALL, createCallbackData(label));
```

2. **Button Creation** (20+ instances):
```cpp
lv_obj_t* btn = lv_btn_create(screen_);
lv_obj_set_size(btn, width, height);
lv_obj_align(btn, LV_ALIGN_TOP_LEFT, x, y);
lv_obj_t* label = lv_label_create(btn);
lv_label_set_text(label, "Text");
lv_obj_center(label);
lv_obj_add_event_cb(btn, callback, LV_EVENT_CLICKED, data);
```

3. **Event Callback Boilerplate**:
```cpp
static void someEventCb(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED/VALUE_CHANGED) {
        CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
        if (data && data->world/manager) {
            // Actual logic here
        }
    }
}
```

## Proposed Architecture

### Hierarchical Component System

#### **Level 1: LVGLBuilder - Primitive UI Builders**
Fundamental LVGL wrappers that eliminate boilerplate:

```cpp
class LVGLBuilder {
public:
    // Fluent interface for sliders
    class SliderBuilder {
    public:
        SliderBuilder(lv_obj_t* parent);
        SliderBuilder& size(int width, int height = 10);
        SliderBuilder& position(int x, int y);
        SliderBuilder& range(int min, int max);
        SliderBuilder& value(int initial_value);
        SliderBuilder& label(const char* text, int label_offset_x = 0);
        SliderBuilder& valueLabel(const char* format = "%.2f"); // Creates auto-updating value display
        SliderBuilder& callback(lv_event_cb_t cb, void* user_data = nullptr);
        lv_obj_t* build(); // Returns the slider object
        
    private:
        lv_obj_t* parent_;
        lv_obj_t* slider_;
        lv_obj_t* value_label_;
        // ... config storage
    };

    // Fluent interface for buttons  
    class ButtonBuilder {
    public:
        ButtonBuilder(lv_obj_t* parent);
        ButtonBuilder& size(int width, int height);
        ButtonBuilder& position(int x, int y);
        ButtonBuilder& text(const char* text);
        ButtonBuilder& toggle(); // Makes it a toggle button
        ButtonBuilder& callback(lv_event_cb_t cb, void* user_data = nullptr);
        lv_obj_t* build();
    };

    // Factory methods
    static SliderBuilder slider(lv_obj_t* parent);
    static ButtonBuilder button(lv_obj_t* parent);
    static lv_obj_t* label(lv_obj_t* parent, const char* text, int x, int y);
};
```

#### **Level 2: Domain-Specific Widgets**
Reusable components that understand the physics simulation:

```cpp
class PhysicsSliderPanel {
    // Creates elasticity, fragmentation, pressure sliders as a group
    void create(lv_obj_t* parent, const LayoutSpec& layout);
    void bindToWorld(WorldInterface* world);
};

class MaterialPicker {
    // Already exists - good example of this level
};

class WorldTypeSelector {
    // Encapsulates WorldA/WorldB switching logic
};

class SimulationControls {
    // Pause/Resume, Reset, Timescale, Screenshot buttons
};
```

#### **Level 3: Layout Managers**
Handle positioning and responsive layout:

```cpp
class ColumnLayout {
    void addWidget(std::unique_ptr<UIWidget> widget);
    void setPosition(int x, int y);
    void layout(); // Calculate positions for all children
};

class GridLayout {
    void addWidget(std::unique_ptr<UIWidget> widget, int row, int col);
};
```

#### **Level 4: Complete UI Assemblies**
Full interfaces built from components:

```cpp
class SimulatorMainUI {
    // Current SimulatorUI refactored
    std::unique_ptr<MaterialPicker> material_picker_;
    std::unique_ptr<PhysicsSliderPanel> physics_controls_;
    std::unique_ptr<SimulationControls> sim_controls_;
    std::unique_ptr<ColumnLayout> layout_;
};

class VisualTestUI {
    // Simplified UI for test framework
    std::unique_ptr<SimulationControls> basic_controls_;
    std::unique_ptr<GridLayout> layout_;
};

class MinimalDemoUI {
    // Future: bare-bones UI for demos
};
```

## LVGLBuilder Usage Examples

### Before (Current SimulatorUI.cpp - 20+ lines):
```cpp
// Elasticity slider - current approach
lv_obj_t* elasticity_label = lv_label_create(screen_);
lv_label_set_text(elasticity_label, "Elasticity");
lv_obj_align(elasticity_label, LV_ALIGN_TOP_LEFT, SLIDER_COLUMN_X, 250);

lv_obj_t* elasticity_value_label = lv_label_create(screen_);
lv_label_set_text(elasticity_value_label, "0.8");
lv_obj_align(elasticity_value_label, LV_ALIGN_TOP_LEFT, SLIDER_COLUMN_X + 110, 250);

lv_obj_t* elasticity_slider = lv_slider_create(screen_);
lv_obj_set_size(elasticity_slider, CONTROL_WIDTH, 10);
lv_obj_align(elasticity_slider, LV_ALIGN_TOP_LEFT, SLIDER_COLUMN_X, 270);
lv_slider_set_range(elasticity_slider, 0, 200);
lv_slider_set_value(elasticity_slider, 80, LV_ANIM_OFF);
lv_obj_add_event_cb(elasticity_slider, elasticitySliderEventCb, LV_EVENT_ALL, 
                   createCallbackData(elasticity_value_label));
```

### After (With LVGLBuilder - 3 lines):
```cpp
auto elasticity_slider = LVGLBuilder::slider(screen_)
    .position(SLIDER_COLUMN_X, 270)
    .size(CONTROL_WIDTH, 10)
    .range(0, 200)
    .value(80)
    .label("Elasticity")
    .valueLabel("%.1f")
    .callback(elasticitySliderEventCb, createCallbackData())
    .build();
```

### Advanced Features

#### Layout Integration:
```cpp
class LayoutBuilder {
public:
    static ColumnLayout column(lv_obj_t* parent, int x, int y, int spacing = 10);
    static GridLayout grid(lv_obj_t* parent, int x, int y, int cols, int spacing = 5);
};

// Usage:
auto physics_column = LayoutBuilder::column(screen_, MAIN_CONTROLS_X, 70, 20);
physics_column.add(LVGLBuilder::slider(screen_).label("Elasticity")...);
physics_column.add(LVGLBuilder::slider(screen_).label("Pressure")...);
physics_column.layout(); // Auto-calculates positions
```

#### Configuration-Driven UI:
```cpp
struct SliderConfig {
    const char* label;
    int min, max, default_val;
    lv_event_cb_t callback;
    const char* format;
};

static const SliderConfig PHYSICS_SLIDERS[] = {
    {"Elasticity", 0, 200, 80, elasticitySliderEventCb, "%.1f"},
    {"Pressure", 0, 1000, 100, pressureSliderEventCb, "%.0f"},
    {"Cohesion", 0, 30000, 15000, cohesionSliderEventCb, "%.0f"}
};

// Create all sliders programmatically
auto column = LayoutBuilder::column(screen_, x, y);
for (const auto& config : PHYSICS_SLIDERS) {
    column.add(LVGLBuilder::slider(screen_)
        .label(config.label)
        .range(config.min, config.max)
        .value(config.default_val)
        .valueLabel(config.format)
        .callback(config.callback, createCallbackData()));
}
```

## Implementation Plan

### Phase 1: Foundation (1-2 iterations)

#### **Step 1: Create LVGLBuilder.h**
- Define the basic class structure and fluent interfaces
- Start with `SliderBuilder` as the most impactful component (30+ sliders in SimulatorUI)
- Include positioning helpers and basic validation

#### **Step 2: Implement SliderBuilder**
- Core functionality: size, position, range, value, callback
- Auto-label creation and positioning
- Value label with format strings
- Integration with existing `CallbackData*` system

#### **Step 3: Proof of Concept Migration**
- Pick **one simple slider** (like "Elasticity") from SimulatorUI.cpp
- Migrate it to use LVGLBuilder
- Verify identical behavior and positioning
- Ensure callbacks work correctly

### Phase 2: Validation (1 iteration)

#### **Step 4: Test Integration**
- Build and run simulator
- Verify migrated slider works identically to original
- Check for memory leaks or callback issues
- Validate that existing tests still pass

#### **Step 5: Expand Coverage**
- Migrate 3-4 more sliders to validate the pattern
- Add `ButtonBuilder` for toggle buttons
- Test with different slider types (pressure, cohesion, etc.)

### Phase 3: Full Migration (2-3 iterations)

#### **Step 6: Batch Migration**
- Migrate all remaining sliders in `createSliders()`
- Migrate buttons in `createControlButtons()` 
- Add layout helpers for positioning consistency

#### **Step 7: Refactor and Cleanup**
- Remove old manual LVGL code
- Consolidate positioning constants
- Add configuration-driven slider creation
- Update header includes and dependencies

### Phase 4: Expansion (Future)

#### **Step 8: Extend to Other UIs**
- Apply LVGLBuilder to TestUI components
- Create shared UI component library
- Document patterns for future UI development

## Implementation Details

### File Structure:
```
src/
├── ui/
│   ├── LVGLBuilder.h       # New fluent interface
│   ├── LVGLBuilder.cpp     # Implementation
│   └── UIComponents.h      # Shared component definitions
├── SimulatorUI.cpp         # Gradually migrated
└── tests/
    └── LVGLBuilder_test.cpp # Unit tests
```

### Build Integration:
- Add to CMakeLists.txt
- No external dependencies (uses existing LVGL)
- Header-only where possible for compile-time optimization

### Risk Mitigation:
1. **Incremental migration** - old and new code coexist during transition
2. **Behavioral validation** - each migrated component tested for identical behavior  
3. **Rollback capability** - Git branches allow easy reverting if issues arise
4. **Minimal API surface** - Start with essential features, expand incrementally

## Expected Outcomes

### After Phase 1-2 (Proof of Concept):
- 1-5 sliders migrated successfully
- Pattern validated and documented
- Foundation ready for scaling

### After Phase 3 (Full Migration):
- SimulatorUI.cpp reduced from ~1780 lines to ~600-800 lines
- All sliders/buttons use consistent patterns
- Positioning logic centralized and maintainable

### After Phase 4 (Expansion):
- Reusable UI library for all interfaces
- New UIs can be created rapidly
- Consistent look/feel across all simulator interfaces

## Benefits

1. **Reusability**: `PhysicsSliderPanel` can be used in main UI, test UI, or debug UI
2. **Consistency**: All sliders behave the same way across different UIs
3. **Maintainability**: Bug fixes in `LVGLBuilder::createSlider()` fix all sliders everywhere
4. **Testability**: Each component can be tested in isolation
5. **Flexibility**: Mix and match components for different use cases
6. **Reduction**: Dramatic reduction in boilerplate code
7. **Type Safety**: Compile-time checking of parameters
8. **Readability**: Intent is clear - `slider().label("Mass").range(0,100)` vs 10 lines of LVGL calls

This incremental approach minimizes risk while delivering immediate value - even migrating just 5-10 sliders would substantially improve code maintainability and establish the foundation for a reusable UI component system.

## Implementation Status & Lessons Learned

### Current Progress (✅ = Complete, 🟡 = In Progress, ⚪ = Pending)

- ✅ **Step 1-2**: LVGLBuilder.h/.cpp created with SliderBuilder, ButtonBuilder, LabelBuilder
- ✅ **Step 3**: Enhanced with `buildOrLog()` and automatic callback integration  
- ✅ **Proof of Concept**: Elasticity slider successfully migrated
- ✅ **Pattern Validation**: Clean 9-line pattern established and tested
- 🟡 **Step 4**: Integration validated - runs successfully with identical behavior
- ⚪ **Step 5-7**: Remaining sliders and buttons to be migrated
- ⚪ **Step 8**: Extension to other UIs (TestUI, future interfaces)

### Final Implementation Pattern

The enhanced LVGLBuilder achieved the clean pattern we envisioned:

```cpp
// Clean 9-line pattern for sliders with automatic error handling
[[maybe_unused]] auto elasticity_slider = LVGLBuilder::slider(screen_)
    .position(SLIDER_COLUMN_X, 270)
    .size(CONTROL_WIDTH, 10)
    .range(0, 200)
    .value(80)
    .label("Elasticity")
    .valueLabel("%.1f")
    .callback(elasticitySliderEventCb, [this](lv_obj_t* value_label) -> void* {
        return createCallbackData(value_label);
    })
    .buildOrLog();
```

### How-To Guide for Future Migrations

#### **Migrating a Slider: Step-by-Step**

1. **Identify the slider code pattern** (usually 15-20 lines):
   ```cpp
   // Find this pattern in SimulatorUI.cpp
   lv_obj_t* some_label = lv_label_create(screen_);
   lv_label_set_text(some_label, "Label Text");
   lv_obj_align(some_label, LV_ALIGN_TOP_LEFT, x, y);
   
   lv_obj_t* some_value_label = lv_label_create(screen_);
   lv_label_set_text(some_value_label, "0.0");
   lv_obj_align(some_value_label, LV_ALIGN_TOP_LEFT, x + 110, y);
   
   lv_obj_t* some_slider = lv_slider_create(screen_);
   lv_obj_set_size(some_slider, CONTROL_WIDTH, 10);
   lv_obj_align(some_slider, LV_ALIGN_TOP_LEFT, x, y + 20);
   lv_slider_set_range(some_slider, min, max);
   lv_slider_set_value(some_slider, default_val, LV_ANIM_OFF);
   lv_obj_add_event_cb(some_slider, someCallback, LV_EVENT_ALL, createCallbackData(some_value_label));
   ```

2. **Extract the parameters**:
   - **Position**: `x, y + 20` (slider position)
   - **Range**: `min, max` values  
   - **Default value**: `default_val`
   - **Label text**: `"Label Text"`
   - **Value format**: Usually `"%.1f"`, `"%.0f"`, or `"%.2f"`
   - **Callback**: `someCallback` function

3. **Replace with LVGLBuilder pattern**:
   ```cpp
   [[maybe_unused]] auto some_slider = LVGLBuilder::slider(screen_)
       .position(x, y + 20)
       .size(CONTROL_WIDTH, 10)
       .range(min, max)
       .value(default_val)
       .label("Label Text")
       .valueLabel("%.1f")  // Adjust format as needed
       .callback(someCallback, [this](lv_obj_t* value_label) -> void* {
           return createCallbackData(value_label);
       })
       .buildOrLog();
   ```

4. **Test the migration**:
   ```bash
   make debug  # Verify it builds
   make run ARGS='-s 10'  # Test basic functionality
   ```

#### **Migrating a Button: Step-by-Step**

1. **Identify button pattern**:
   ```cpp
   lv_obj_t* some_btn = lv_btn_create(screen_);
   lv_obj_set_size(some_btn, width, height);
   lv_obj_align(some_btn, LV_ALIGN_TOP_LEFT, x, y);
   lv_obj_t* some_label = lv_label_create(some_btn);
   lv_label_set_text(some_label, "Button Text");
   lv_obj_center(some_label);
   lv_obj_add_event_cb(some_btn, someCallback, LV_EVENT_CLICKED, createCallbackData());
   ```

2. **Replace with ButtonBuilder**:
   ```cpp
   [[maybe_unused]] auto some_btn = LVGLBuilder::button(screen_)
       .position(x, y)
       .size(width, height)
       .text("Button Text")
       .callback(someCallback, createCallbackData())
       .buildOrLog();
   ```

#### **Common Gotchas & Solutions**

1. **Unused variable warnings**: Use `[[maybe_unused]]` attribute
2. **Callback data timing**: Value labels are created during `build()`, so lambda captures work correctly
3. **Position adjustments**: Labels default to 20px above sliders - may need tweaking
4. **Format strings**: Common formats are `"%.0f"` (integers), `"%.1f"` (1 decimal), `"%.2f"` (2 decimals)

#### **Validation Checklist**

For each migrated slider/button:
- [ ] Code builds without warnings
- [ ] Application runs without errors  
- [ ] UI element appears in correct position
- [ ] Interaction (clicking/dragging) works
- [ ] Callback updates value labels correctly
- [ ] No behavioral differences from original

### Architecture Decisions Made

1. **Result-based error handling**: Using `Result<T, std::string>` for explicit error management
2. **Fluent interface**: Method chaining for readable configuration
3. **Factory pattern**: Callback data factory functions for automatic integration
4. **RAII approach**: Configure → build() → use pattern
5. **Logging integration**: Errors logged at utility level, not call sites

### Files Modified

- `src/ui/LVGLBuilder.h` - Header with fluent interfaces (173 lines)
- `src/ui/LVGLBuilder.cpp` - Implementation with error handling (456 lines)
- `src/SimulatorUI.cpp` - Enhanced with #include and one migrated slider
- `CMakeLists.txt` - Added LVGLBuilder.cpp to both executables

### Next Session Tasks

1. **Migrate 5-10 more sliders** using the established pattern
2. **Migrate simple buttons** (Reset, Pause, etc.)
3. **Consider layout helpers** for consistent positioning
4. **Create unit tests** for LVGLBuilder components
5. **Document patterns** for TestUI and future UI development

The foundation is solid and the pattern is proven. Future migrations should be straightforward following the how-to guide above.
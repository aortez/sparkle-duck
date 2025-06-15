# JSON Serialization Test Plan for Cell System

## Overview

This document outlines the test plan for implementing JSON serialization support for the Sparkle Duck cell system. The goal is to enable complete world state dumps as JSON for debugging, save/load functionality, and assertion crash reporting.

## Architecture Goals

### Primary Objectives
1. **Complete State Serialization**: Serialize entire world state including all cell data
2. **Cross-System Compatibility**: Support both RulesA (Cell) and RulesB (CellB) through WorldState
3. **Crash Dump Integration**: Enable JSON dumps on assertion failures
4. **Save/Load Functionality**: Foundation for world persistence

### Design Principles
- **Leverage Existing Infrastructure**: Use already-available RapidJSON through LVGL/ThorVG
- **Maintain Polymorphism**: Work through existing CellInterface and WorldState architecture
- **Backward Compatibility**: Don't break existing toString() methods or WorldState conversion
- **Performance Conscious**: Efficient serialization for large grids (up to 500x500 cells)

## Implementation Strategy

### Phase 1: Core JSON Infrastructure
**Target Classes**: `Vector2d`, `MaterialType`, `WorldState`

#### 1.1 Vector2d JSON Support
```cpp
// Add to Vector2d class
nlohmann::json toJson() const;
static Vector2d fromJson(const nlohmann::json& j);
```

**Test Cases**:
- Serialize/deserialize normal vectors
- Handle edge cases: zero vectors, large values, negative coordinates
- Validate floating-point precision preservation

#### 1.2 MaterialType JSON Support
```cpp
// Add utility functions
nlohmann::json materialTypeToJson(MaterialType type);
MaterialType materialTypeFromJson(const nlohmann::json& j);
```

**Test Cases**:
- All 8 material types (AIR, DIRT, WATER, WOOD, SAND, METAL, LEAF, WALL)
- Invalid material type handling
- Case sensitivity and string format validation

#### 1.3 WorldState JSON Serialization
```cpp
// Add to WorldState class
nlohmann::json toJson() const;
static WorldState fromJson(const nlohmann::json& j);
```

**Test Cases**:
- Empty world serialization
- Full world with mixed materials
- Large grids (performance testing)
- Physics parameter preservation
- Timestep and metadata preservation

### Phase 2: Cell-Level JSON Support

#### 2.1 CellData JSON Support
```cpp
struct CellData {
    // Add JSON methods
    nlohmann::json toJson() const;
    static CellData fromJson(const nlohmann::json& j);
};
```

**Test Cases**:
- All material types with various fill ratios
- Complex physics states (velocity, COM, pressure)
- Boundary value testing (0.0, 1.0 fill ratios)
- Invalid state handling

#### 2.2 Direct Cell JSON (Optional Enhancement)
```cpp
// Add to Cell and CellB classes
nlohmann::json toJson() const;
static Cell/CellB fromJson(const nlohmann::json& j);
```

**Test Cases**:
- Cell (RulesA): Multi-material states, complex physics
- CellB (RulesB): Pure material states, simplified physics
- Round-trip conversion accuracy
- Performance with grid-scale serialization

### Phase 3: World-Level Integration

#### 3.1 World JSON Export
```cpp
// Add to World and WorldB classes
nlohmann::json exportToJson() const;
void importFromJson(const nlohmann::json& j);
```

**Test Cases**:
- Complete world state export/import
- Physics system switching (WorldA ↔ WorldB via JSON)
- Large simulation state preservation
- Drag state and temporary data handling

#### 3.2 Crash Dump Integration
```cpp
// Add to assertion handling
void dumpWorldStateToJson(const char* filename);
void installJsonCrashHandler();
```

**Test Cases**:
- Assertion failure triggers JSON dump
- File write permissions and error handling
- JSON structure validation in crash dumps
- Performance under crash conditions

## Test Framework Design

### Unit Test Structure

#### 3.1 Core JSON Tests (`JSONSerialization_test.cpp`)
```cpp
class JSONSerializationTest : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;
    
    // Helper methods
    void validateRoundTrip(const WorldState& original);
    void checkJsonStructure(const nlohmann::json& j);
};

// Test categories
TEST_F(JSONSerializationTest, Vector2dSerialization)
TEST_F(JSONSerializationTest, MaterialTypeSerialization)  
TEST_F(JSONSerializationTest, CellDataSerialization)
TEST_F(JSONSerializationTest, WorldStateSerialization)
```

#### 3.2 Integration Tests (`WorldJSONExport_test.cpp`)
```cpp
class WorldJSONExportTest : public ::testing::Test {
protected:
    std::unique_ptr<World> worldA;
    std::unique_ptr<WorldB> worldB;
    
    void setupTestScenarios();
    void validateExportImport(const nlohmann::json& exported);
};

TEST_F(WorldJSONExportTest, EmptyWorldExport)
TEST_F(WorldJSONExportTest, ComplexPhysicsStateExport)
TEST_F(WorldJSONExportTest, LargeGridPerformance)
TEST_F(WorldJSONExportTest, CrossSystemCompatibility)
```

#### 3.3 Crash Dump Tests (`JSONCrashDump_test.cpp`)
```cpp
class JSONCrashDumpTest : public ::testing::Test {
protected:
    void simulateAssertion();
    void validateDumpFile(const std::string& filename);
};

TEST_F(JSONCrashDumpTest, AssertionTriggersJsonDump)
TEST_F(JSONCrashDumpTest, DumpFileStructureValid)
TEST_F(JSONCrashDumpTest, FileWriteErrorHandling)
```

### Visual Test Integration

#### 3.4 Visual JSON Tests
**Integration with existing visual test framework**:
```cpp
// In WorldBVisual_test.cpp
TEST_F(WorldBVisualTest, JsonExportVisualVerification) {
    // 1. Setup complex world state
    // 2. Export to JSON
    // 3. Clear world
    // 4. Import from JSON
    // 5. Visual comparison of before/after
}
```

## JSON Schema Design

### Schema Structure
```json
{
  "world_state": {
    "metadata": {
      "version": "1.0",
      "physics_system": "RulesB",
      "timestamp": "2024-12-15T10:30:00Z",
      "generator": "SparkluDuck v1.0"
    },
    "grid": {
      "width": 800,
      "height": 600,
      "timestep": 1250
    },
    "physics": {
      "gravity": 0.1,
      "timescale": 1.0,
      "elasticity_factor": 0.5,
      "pressure_scale": 1.0
    },
    "cells": [
      {
        "x": 0, "y": 0,
        "material_mass": 0.75,
        "dominant_material": "DIRT",
        "velocity": {"x": 0.0, "y": 0.1},
        "com": {"x": 0.0, "y": 0.0},
        "pressure": 0.0
      }
    ]
  }
}
```

### Schema Validation
- JSON Schema file for validation
- Version compatibility checking
- Required vs optional fields
- Value range validation

## Performance Requirements

### Benchmarks
- **Small Grid** (100x100): < 50ms serialization
- **Large Grid** (500x500): < 500ms serialization  

### Memory Usage
- Streaming JSON generation for large grids
- Memory-efficient parsing for import
- Garbage collection considerations

## Error Handling Strategy

### Serialization Errors
```cpp
enum class JsonSerializationError {
    SUCCESS,
    INVALID_MATERIAL_TYPE,
    INVALID_VECTOR_DATA,
    GRID_SIZE_MISMATCH,
    VERSION_INCOMPATIBLE,
    FILE_WRITE_ERROR,
    MEMORY_ALLOCATION_ERROR
};
```

### Recovery Mechanisms
- Partial state recovery on import errors
- Fallback to default values for missing fields
- Validation before full world state replacement
- Backup creation before import operations

## Implementation Timeline

### Week 1: Foundation
- [ ] Vector2d JSON support + tests
- [ ] MaterialType JSON support + tests
- [ ] Basic JSON infrastructure setup

### Week 2: Core Serialization
- [ ] WorldState JSON methods + tests
- [ ] CellData JSON support + tests
- [ ] Performance benchmarking framework

### Week 3: Integration
- [ ] World-level JSON export/import
- [ ] Crash dump integration
- [ ] Cross-system compatibility testing

### Week 4: Polish
- [ ] Visual test integration
- [ ] Performance optimization
- [ ] Documentation and examples

## Success Criteria

### Functional Requirements
✅ Complete world state serializable to/from JSON  
✅ All cell types (Cell/CellB) supported through WorldState  
✅ Assertion failures automatically generate JSON dumps  
✅ Round-trip accuracy: export→import preserves simulation state  
✅ Cross-system compatibility: RulesA ↔ RulesB via JSON  

### Performance Requirements
✅ Large grid serialization completes within time limits  
✅ Memory usage remains reasonable during JSON operations  
✅ No performance regression in normal simulation  

### Quality Requirements
✅ 100% test coverage for JSON serialization code  
✅ All edge cases handled gracefully  
✅ Comprehensive error reporting and recovery  
✅ Integration with existing debugging tools  

## Future Enhancements

### Save/Load System
- User-facing save/load functionality in SimulatorUI
- JSON file format standardization

### Network Serialization
- JSON-based world state sharing
- Network protocol for multi-client simulations
- WebRTC integration with JSON state transfer

### Debugging Tools
- JSON diff tools for state comparison
- Timeline export for animation replay
- State inspection tools for development

---

**Document Version**: 1.0  
**Last Updated**: 2024-12-15  
**Status**: Planning Phase
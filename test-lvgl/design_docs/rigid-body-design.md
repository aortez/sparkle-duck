# Rigid Body Design for Grid-Based Physics

## The Problem

Currently, rigid materials (WOOD, METAL) are represented as individual particles with cohesion forces. This causes issues:
- Horizontal structures fall apart (no normal force from below → no friction)
- High cohesion creates COM drift and pressure waves
- No concept of structural integrity or breaking stress

**Goal:** Enable rigid structures to maintain their shape under load, while still allowing breaking/fracture under extreme stress.

## Core Design Questions

### 1. Structure Identification
**Q:** How do we identify what constitutes a "rigid structure"?

**Options:**
- A) Flood-fill connected rigid cells of same material
- B) Flood-fill rigid cells of same organism_id (for trees)
- C) Explicit structure tagging (manual or procedural)

**Consideration:** Should a pile of METAL blocks form one structure, or separate structures? What about WOOD cells that are adjacent but from different trees?

### 2. Structure Representation
**Q:** How do we represent a rigid structure in data?

**Minimal Approach:**
```cpp
struct RigidStructure {
    std::vector<Vector2i> cells;        // Grid positions in structure
    Vector2d center_of_mass;            // Continuous coords
    double total_mass;
    Vector2d velocity;                  // Linear velocity
    uint32_t organism_id;               // 0 for non-organism structures
};
```

**Full Rigid Body:**
```cpp
struct RigidBody {
    std::vector<Vector2i> cells;        // Grid positions
    Vector2d position;                  // Continuous COM position
    double rotation;                    // Angle in radians
    double total_mass;
    double moment_of_inertia;
    Vector2d linear_velocity;
    double angular_velocity;
    uint32_t organism_id;
    MaterialType material;
};
```

**Decision:** Start minimal (no rotation), add complexity later?

### 3. Grid Integration
**Q:** How does the rigid body interact with the grid representation?

**Two-way mapping:**
- **Body → Grid:** Update grid cells based on body position/rotation
- **Grid → Body:** Gather forces from grid interactions

**Challenges:**
- Partial cell occupancy (body spans multiple cells)
- COM placement within cells
- Material fill_ratio calculation

### 4. Force Gathering
**Q:** How do we collect forces from grid interactions and apply to rigid body?

**Approach:**
1. Grid interactions (pressure, collisions, etc.) create forces on individual cells
2. Sum forces across all cells in rigid body
3. Calculate torque for off-center forces: τ = r × F
4. Apply to rigid body: F_total, τ_total

**Question:** Do we still run normal physics on individual cells, then aggregate? Or bypass cell-level physics entirely?

### 5. Movement & Integration
**Q:** How do we update rigid body position and update grid?

**Simple approach (translation only):**
```
1. Gather forces from grid
2. F = ma → update velocity
3. Update center of mass: COM += velocity * dt
4. Move material in grid cells to follow COM
```

**Full approach (with rotation):**
```
1. Gather forces and torques
2. F = ma → update linear_velocity
3. τ = Iα → update angular_velocity
4. Update position and rotation
5. Remap body shape to grid based on new pose
```

### 6. Collision Detection
**Q:** How do we handle rigid body collisions?

**Body-Body Collisions:**
- Detect overlap in grid
- Calculate contact points and normals
- Apply impulses to resolve

**Body-Particle Collisions:**
- Rigid body cells interact with fluid/loose particles
- Treat as grid interactions?

### 7. Breaking & Fracture
**Q:** When and how do structures break?

**Stress-based approach:**
1. Track stress/strain in structure
2. When stress exceeds material strength → fracture
3. Split structure into fragments

**Force-based approach:**
1. If total force on structure > threshold → break weakest connection
2. Flood-fill to identify new separate structures

**Simple approach:**
1. Just don't break - structures stay rigid until manually destroyed
2. Add fracture later

## Proposed Phased Implementation

### Phase 1: Minimal Rigid Structures (Translation Only)
- Identify connected rigid cells via flood fill
- Store as RigidStructure (COM, velocity, cells)
- Velocity averaging across structure
- No rotation, no fracture
- Goal: Horizontal branches stop falling

### Phase 2: Force Integration
- Gather forces from grid properly
- Apply F=ma to structures
- Interact with pressure, fluids
- Goal: Structures respond to external forces realistically

### Phase 3: Rotation & Full Rigid Body
- Add rotation angle and angular velocity
- Calculate moment of inertia
- Apply torques
- Remap to grid with rotation
- Goal: Structures can rotate, tip over

### Phase 4: Fracture Mechanics
- Track stress in structures
- Break connections under stress
- Fragment into multiple bodies
- Goal: Structures can break apart

## Open Questions

1. **Performance:** How many rigid bodies can we track? 100s? 1000s?
2. **Organism integration:** Should organism bones replace or complement rigid bodies?
3. **Material mixing:** What if a structure has both WOOD and ROOT cells?
4. **Grid consistency:** How do we ensure grid state stays consistent with body state?
5. **Serialization:** How do we save/load rigid body state?

## Alternative: Constraint-Based Approach

Instead of tracking rigid bodies separately, use constraint solving:
- Each rigid connection is a distance constraint
- Each timestep: apply forces, then project onto constraints
- XPBD (Extended Position Based Dynamics) style
- Simpler implementation, more integrated with grid

**Trade-off:** Less physically accurate, but easier to implement and integrate.

---

*Notes for discussion...*

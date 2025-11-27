# Server-Side Rendering Design

## Problem Statement

Current architecture serializes full WorldData (all Cell fields) and sends to UI for rendering.

**Performance bottleneck (150×150 grid):**
- Serialization: 6.77 ms (28% of frame time)
- Payload: 3.58 MB per frame (~166 bytes per cell)
- Network: 0.94 ms
- **Total: 7.71 ms overhead** limiting FPS

## Proposed Solution

Move rendering to server, send only rendered pixels to UI.

## Architecture Comparison

### Current: Client-Side Rendering
```
Server: Physics → Serialize WorldData (3.73 MB) → Send
UI: Receive → Deserialize → Render → Display
```

### Proposed: Server-Side Rendering
```
Server: Physics → Render to framebuffer → Send pixels (90-1440 KB) →
UI: Receive → Display framebuffer directly
```

## Size Analysis

### Current Payload (per cell)
```
material_type                    4 bytes
fill_ratio                       8 bytes
com                             16 bytes
velocity                        16 bytes
organism_id                      4 bytes
pressure (3 components)         24 bytes
pressure_gradient               16 bytes
accumulated forces (3 types)    48 bytes
pending_force                   16 bytes
cached_friction                  8 bytes
support flags                    2 bytes
-------------------------------------------
Total: ~166 bytes per cell

150×150 = 22,500 cells × 166 = 3.73 MB
```

### Proposed: Rendered Framebuffer

**Option 1: 1 pixel per cell (150×150)**
- RGBA: 4 bytes × 22,500 = 90 KB
- **41× smaller!**
- No zoom, minimal detail

**Option 2: 4×4 pixels per cell (600×600)**
- RGBA: 4 bytes × 360,000 = 1.44 MB
- **2.6× smaller**
- Good detail, debug overlays possible

**Option 3: Compressed (PNG/LZ4)**
- 600×600 RGBA → ~200-500 KB compressed
- **7-18× smaller**
- Adds compression CPU cost

## Implementation Plan

### Phase 1: Basic Pixel Renderer (Server)

**Create HeadlessRenderer class:**
```cpp
class HeadlessRenderer {
public:
    std::vector<uint32_t> render(const WorldData& data, RenderMode mode);

private:
    uint32_t getMaterialColor(MaterialType type);
    void renderCell(const Cell& cell, uint32_t* pixels, int x, int y);
};
```

**Port from CellRenderer.cpp (lines 300-377):**
- Color mapping (getMaterialColor)
- Alpha blending based on fill_ratio
- Direct ARGB32 buffer writes
- **No LVGL dependency**

### Phase 2: Debug Visualizations (Server)

**Add optional overlays:**
- COM indicators (yellow dots)
- Velocity vectors (green lines)
- Pressure borders (red/magenta)
- Force vectors (colored arrows)

**Implementation:**
- Simple line/rectangle drawing to framebuffer
- No LVGL - just pixel manipulation
- Toggleable via render flags

### Phase 3: Network Protocol Update

**New binary message format:**
```
[Header: 16 bytes]
  - width: uint32_t
  - height: uint32_t
  - format: uint32_t (RGBA, RGB, etc.)
  - flags: uint32_t (debug mode, compression, etc.)

[Pixel data: width × height × 4 bytes]
  - Raw ARGB32 framebuffer
```

**Server changes:**
- Replace zpp_bits serialization with raw buffer send
- Add render step before network send

**UI changes:**
- Receive binary blob
- Create LVGL canvas from buffer directly
- Remove Cell deserialization

## Performance Projection

### For 150×150 grid @ 600×600 resolution:

**Current:**
- Serialization: 6.77 ms
- Network: 0.94 ms
- Payload: 3.73 MB

**Projected (uncompressed):**
- Rendering: ~2-3 ms (simpler than current serialization)
- Network: ~0.40 ms (smaller payload)
- Payload: 1.44 MB (2.6× smaller)
- **Savings: ~5 ms** → **50+ FPS** (from 35 FPS)

**Projected (with compression):**
- Rendering + compress: ~3-5 ms
- Network: ~0.10 ms
- Payload: 200-500 KB (7-18× smaller)
- **Savings: ~3-4 ms** → **45+ FPS**

## Trade-offs

### Advantages
1. **Much smaller payloads** (2-50× reduction)
2. **Faster serialization** (simple memcpy vs zpp_bits)
3. **Simpler UI** (just display bitmap)
4. **Server controls all visuals** (consistent debug views)
5. **Network bandwidth** saved significantly

### Disadvantages
1. **Loses interactive data** (can't query individual cells easily)
2. **No sub-cell precision** (COM visualization limited to pixels)
3. **Additional server work** (rendering cost added)
4. **Initial implementation effort** (~1-2 days)

## Hybrid Approach (Recommended)

**Default mode: Server-rendered bitmap**
- Fast, efficient
- Good for real-time visualization

**Debug mode: Full WorldData on demand**
- Click cell → request full Cell data for that cell
- Toggle "full data mode" → send complete WorldData
- Best of both worlds

## Implementation Complexity

**Estimated effort:**
- HeadlessRenderer (basic): ~4 hours
- Network protocol update: ~2 hours
- UI integration: ~2 hours
- Debug overlays: ~4 hours
- **Total: ~1-2 days**

**High value, moderate effort.**

## Next Steps

1. Prototype HeadlessRenderer with basic color rendering
2. Measure actual rendering performance
3. Update network protocol
4. Integrate with UI
5. Add debug visualizations incrementally

## Conclusion

Server-side rendering would provide **2-50× reduction** in network payload and **~5 ms** frame time savings, pushing FPS from 35 to 50+ for 150×150 grids. The trade-off is losing some interactive data access, but a hybrid approach can preserve that when needed.

**Recommendation: Implement basic server rendering as default, keep full data mode as fallback.**

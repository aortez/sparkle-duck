#include "WorldBSupportCalculator.h"
#include "WorldB.h"
#include "CellB.h"
#include "MaterialType.h"
#include "Vector2i.h"
#include "spdlog/spdlog.h"
#include <cmath>
#include <algorithm>
#include <queue>
#include <set>
#include <array>

WorldBSupportCalculator::WorldBSupportCalculator(const WorldB& world)
    : world_(world)
{
}

const CellB& WorldBSupportCalculator::getCellAt(uint32_t x, uint32_t y) const
{
    // Direct access to CellB through WorldB
    return world_.at(x, y);
}

bool WorldBSupportCalculator::isValidCell(int x, int y) const
{
    return x >= 0 && y >= 0 && 
           static_cast<uint32_t>(x) < world_.getWidth() && 
           static_cast<uint32_t>(y) < world_.getHeight();
}

bool WorldBSupportCalculator::hasVerticalSupport(uint32_t x, uint32_t y) const
{
    if (!isValidCell(static_cast<int>(x), static_cast<int>(y))) {
        spdlog::trace("hasVerticalSupport({},{}) = false (invalid cell)", x, y);
        return false;
    }
    
    const CellB& cell = getCellAt(x, y);
    if (cell.isEmpty()) {
        spdlog::trace("hasVerticalSupport({},{}) = false (empty cell)", x, y);
        return false;
    }
    
    // Check if already at ground level
    if (y == world_.getHeight() - 1) {
        spdlog::trace("hasVerticalSupport({},{}) = true (at ground level)", x, y);
        return true;
    }
    
    // Check cells directly below for continuous material (no gaps allowed)
    for (uint32_t dy = 1; dy <= MAX_VERTICAL_SUPPORT_DISTANCE; dy++) {
        uint32_t support_y = y + dy;
        
        // If we reach beyond the world boundary, no material support available
        if (support_y >= world_.getHeight()) {
            spdlog::info("hasVerticalSupport({},{}) = false (reached world boundary at distance {}, no material below)", x, y, dy);
            break;
        }
        
        const CellB& below = getCellAt(x, support_y);
        if (!below.isEmpty()) {
            // RECURSIVE SUPPORT CHECK: Supporting block must itself be supported
            bool supporting_block_supported = hasVerticalSupport(x, support_y);
            if (supporting_block_supported) {
                spdlog::trace("hasVerticalSupport({},{}) = true (material {} below at distance {} is itself supported)", 
                             x, y, getMaterialName(below.getMaterialType()), dy);
                return true;
            } else {
                spdlog::trace("hasVerticalSupport({},{}) = false (material {} below at distance {} is unsupported)", 
                             x, y, getMaterialName(below.getMaterialType()), dy);
                return false;
            }
        } else {
            // CRITICAL FIX: Stop at first empty cell - no support through gaps
            spdlog::trace("hasVerticalSupport({},{}) = false (empty cell at distance {}, no continuous support)", x, y, dy);
            return false;
        }
    }
    
    spdlog::info("hasVerticalSupport({},{}) = false (no material below within {} cells)", 
                 x, y, MAX_VERTICAL_SUPPORT_DISTANCE);
    return false;
}

bool WorldBSupportCalculator::hasHorizontalSupport(uint32_t x, uint32_t y) const
{
    if (!isValidCell(static_cast<int>(x), static_cast<int>(y))) {
        spdlog::trace("hasHorizontalSupport({},{}) = false (invalid cell)", x, y);
        return false;
    }
    
    const CellB& cell = getCellAt(x, y);
    if (cell.isEmpty()) {
        spdlog::trace("hasHorizontalSupport({},{}) = false (empty cell)", x, y);
        return false;
    }
    
    const MaterialProperties& cell_props = getMaterialProperties(cell.getMaterialType());
    
    // Check immediate neighbors only (no BFS for horizontal support)
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0) continue; // Skip self
            
            int nx = static_cast<int>(x) + dx;
            int ny = static_cast<int>(y) + dy;
            
            if (!isValidCell(nx, ny)) continue;
            
            const CellB& neighbor = getCellAt(nx, ny);
            if (neighbor.isEmpty()) continue;
            
            const MaterialProperties& neighbor_props = getMaterialProperties(neighbor.getMaterialType());
            
            // Check for rigid support: high-density neighbor with strong adhesion
            if (neighbor_props.density > RIGID_DENSITY_THRESHOLD) {
                // Calculate mutual adhesion between materials
                double mutual_adhesion = std::sqrt(cell_props.adhesion * neighbor_props.adhesion);
                
                if (mutual_adhesion > STRONG_ADHESION_THRESHOLD) {
                    spdlog::trace("hasHorizontalSupport({},{}) = true (rigid {} neighbor with adhesion {:.3f})", 
                                 x, y, getMaterialName(neighbor.getMaterialType()), mutual_adhesion);
                    return true;
                }
            }
        }
    }
    
    spdlog::trace("hasHorizontalSupport({},{}) = false (no rigid neighbors with strong adhesion)", x, y);
    return false;
}

bool WorldBSupportCalculator::hasStructuralSupport(uint32_t x, uint32_t y) const
{
    if (!isValidCell(static_cast<int>(x), static_cast<int>(y))) {
        spdlog::trace("hasStructuralSupport({},{}) = false (invalid cell)", x, y);
        return false;
    }
    
    const CellB& cell = getCellAt(x, y);
    
    // Empty cells provide no support
    if (cell.isEmpty()) {
        spdlog::trace("hasStructuralSupport({},{}) = false (empty cell)", x, y);
        return false;
    }
    
    // Support conditions (in order of priority):
    
    // 1. WALL material is always considered structural support
    if (cell.getMaterialType() == MaterialType::WALL) {
        spdlog::trace("hasStructuralSupport({},{}) = true (WALL material)", x, y);
        return true;
    }
    
    // 2. Bottom edge of world (ground) provides support
    if (y == world_.getHeight() - 1) {
        spdlog::trace("hasStructuralSupport({},{}) = true (ground level, y={}, height={})", x, y, y, world_.getHeight());
        return true;
    }
    
    // 3. High-density materials provide structural support
    // METAL has density 7.8, so it acts as structural anchor
    const MaterialProperties& props = getMaterialProperties(cell.getMaterialType());
    if (props.density > 5.0) {
        spdlog::trace("hasStructuralSupport({},{}) = true (high density {:.1f})", x, y, props.density);
        return true;
    }
    
    // 4. NEW: Limited-depth BFS to find nearby structural support
    // Check within MAX_SUPPORT_DISTANCE (10 cells) for ground/walls/anchors
    std::queue<std::pair<Vector2i, int>> search_queue; // position, distance
    std::set<std::pair<int, int>> visited; // Track visited cells to avoid cycles
    
    search_queue.push({{static_cast<int>(x), static_cast<int>(y)}, 0});
    visited.insert({static_cast<int>(x), static_cast<int>(y)});
    
    while (!search_queue.empty()) {
        auto [pos, distance] = search_queue.front();
        search_queue.pop();
        
        // Skip if we've exceeded maximum search distance
        if (distance >= static_cast<int>(MAX_SUPPORT_DISTANCE)) {
            continue;
        }
        
        // Check all 8 neighbors from current position
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                if (dx == 0 && dy == 0) continue; // Skip self
                
                int nx = pos.x + dx;
                int ny = pos.y + dy;
                
                if (!isValidCell(nx, ny) || visited.count({nx, ny})) {
                    continue;
                }
                
                visited.insert({nx, ny});
                const CellB& neighbor = getCellAt(nx, ny);
                
                // Skip empty cells
                if (neighbor.isEmpty()) {
                    continue;
                }
                
                // Check for immediate structural support
                if (neighbor.getMaterialType() == MaterialType::WALL ||
                    ny == static_cast<int>(world_.getHeight()) - 1) { // Ground level
                    
                    spdlog::trace("hasStructuralSupport({},{}) = true (found {} at distance {})", 
                                 x, y, neighbor.getMaterialType() == MaterialType::WALL ? "WALL" : "GROUND", distance + 1);
                    return true;
                }
                
                // High-density materials act as anchors
                const MaterialProperties& neighbor_props = getMaterialProperties(neighbor.getMaterialType());
                if (neighbor_props.density > 5.0) {
                    spdlog::trace("hasStructuralSupport({},{}) = true (found high-density {} at distance {})", 
                                 x, y, getMaterialName(neighbor.getMaterialType()), distance + 1);
                    return true;
                }
                
                // Continue BFS only through connected materials (same type)
                // This prevents "floating through air" false positives
                if (neighbor.getMaterialType() == cell.getMaterialType() && 
                    neighbor.getFillRatio() > MIN_MATTER_THRESHOLD) {
                    search_queue.push({{nx, ny}, distance + 1});
                }
            }
        }
    }
    
    spdlog::trace("hasStructuralSupport({},{}) = false (no support found within {} cells)", x, y, MAX_SUPPORT_DISTANCE);
    return false;
}

double WorldBSupportCalculator::calculateDistanceToSupport(uint32_t x, uint32_t y) const
{
    spdlog::info("calculateDistanceToSupport({},{}) called", x, y);
    const CellB& cell = getCellAt(x, y);
    if (cell.isEmpty()) {
        spdlog::info("calculateDistanceToSupport({},{}) = {} (empty cell)", x, y, MAX_SUPPORT_DISTANCE);
        return MAX_SUPPORT_DISTANCE; // No material = no support needed
    }
    
    MaterialType material = cell.getMaterialType();
    
    // Use simpler 2D array for distance tracking (avoid Vector2i comparisons)
    std::vector<std::vector<int>> distances(world_.getWidth(), std::vector<int>(world_.getHeight(), -1));
    std::queue<std::pair<uint32_t, uint32_t>> queue;
    
    queue.push({x, y});
    distances[x][y] = 0;
    
    // 8-directional neighbor offsets (including diagonals)
    static const std::array<std::pair<int, int>, 8> directions = {{
        {-1, -1}, {-1, 0}, {-1, 1},
        { 0, -1},          { 0, 1},
        { 1, -1}, { 1, 0}, { 1, 1}
    }};
    
    while (!queue.empty()) {
        auto current = queue.front();
        queue.pop();
        
        uint32_t cx = current.first;
        uint32_t cy = current.second;
        
        // Check if current position has structural support
        if (hasStructuralSupport(cx, cy)) {
            int distance = distances[cx][cy];
            spdlog::trace("Support found for material at ({},{}) - distance: {}", x, y, distance);
            return static_cast<double>(distance);
        }
        
        // Limit search depth to prevent infinite loops and improve performance
        if (distances[cx][cy] >= static_cast<int>(MAX_SUPPORT_DISTANCE)) {
            continue;
        }
        
        // Explore all 8 neighbors
        for (const auto& dir : directions) {
            int nx = static_cast<int>(cx) + dir.first;
            int ny = static_cast<int>(cy) + dir.second;
            
            if (nx >= 0 && ny >= 0 && 
                nx < static_cast<int>(world_.getWidth()) && ny < static_cast<int>(world_.getHeight()) &&
                distances[nx][ny] == -1) { // Not visited
                
                const CellB& nextCell = getCellAt(nx, ny);
                
                // Follow paths through connected material
                // Either same material, or structural support material (metal, walls)
                bool canConnect = false;
                if (nextCell.getMaterialType() == material && 
                    nextCell.getFillRatio() > MIN_MATTER_THRESHOLD) {
                    canConnect = true; // Same material connection
                } else if (!nextCell.isEmpty() && hasStructuralSupport(nx, ny)) {
                    canConnect = true; // Structural support connection (metal, walls, etc.)
                }
                
                if (canConnect) {
                    distances[nx][ny] = distances[cx][cy] + 1;
                    queue.push({static_cast<uint32_t>(nx), static_cast<uint32_t>(ny)});
                }
            }
        }
    }
    
    // No support found within search radius
    spdlog::trace("No support found for material at ({},{}) within distance {}", x, y, MAX_SUPPORT_DISTANCE);
    return MAX_SUPPORT_DISTANCE;
}
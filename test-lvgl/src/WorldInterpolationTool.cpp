#include "WorldInterpolationTool.h"
#include "WorldInterface.h"
#include "WorldFactory.h"
#include "World.h"
#include "CellB.h" 
#include "Cell.h"
#include "MaterialType.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <stdexcept>
#include <cassert>

// =================================================================
// PUBLIC INTERFACE
// =================================================================

bool WorldInterpolationTool::resizeWorldWithBilinearFiltering(WorldInterface& world, 
                                                            uint32_t newWidth, 
                                                            uint32_t newHeight)
{
    // This method is now deprecated - worlds should call resizeGrid directly
    // which will use the generateInterpolatedCells* methods
    spdlog::warn("resizeWorldWithBilinearFiltering is deprecated - use world.resizeGrid() directly");
    world.resizeGrid(newWidth, newHeight);
    return true;
}

std::vector<CellB> WorldInterpolationTool::generateInterpolatedCellsB(
    const std::vector<CellB>& oldCells,
    uint32_t oldWidth, uint32_t oldHeight,
    uint32_t newWidth, uint32_t newHeight)
{
    assert(!oldCells.empty() && "Old cells vector must not be empty");
    assert(oldCells.size() == oldWidth * oldHeight && "Old cells size must match dimensions");
    assert(newWidth > 0 && newHeight > 0 && "New dimensions must be positive");
    
    std::vector<CellB> newCells;
    newCells.reserve(newWidth * newHeight);
    
    // Calculate scaling factors
    double scaleX = static_cast<double>(oldWidth) / static_cast<double>(newWidth);
    double scaleY = static_cast<double>(oldHeight) / static_cast<double>(newHeight);
    
    spdlog::debug("Interpolating CellB grid: {}x{} -> {}x{}, scale factors: {:.3f}x{:.3f}", 
                  oldWidth, oldHeight, newWidth, newHeight, scaleX, scaleY);
    
    // Generate interpolated cells for each position in the new grid
    for (uint32_t newY = 0; newY < newHeight; ++newY) {
        for (uint32_t newX = 0; newX < newWidth; ++newX) {
            // Map destination cell to source coordinates
            double srcX = (static_cast<double>(newX) + 0.5) * scaleX - 0.5;
            double srcY = (static_cast<double>(newY) + 0.5) * scaleY - 0.5;
            
            // Get integer source coordinates and fractional parts
            int srcX0 = static_cast<int>(std::floor(srcX));
            int srcY0 = static_cast<int>(std::floor(srcY));
            int srcX1 = srcX0 + 1;
            int srcY1 = srcY0 + 1;
            
            double fx = srcX - srcX0;
            double fy = srcY - srcY0;
            
            // Clamp to valid grid bounds
            clampToGrid(srcX0, srcY0, oldWidth, oldHeight);
            clampToGrid(srcX1, srcY1, oldWidth, oldHeight);
            
            // Get the 4 neighboring cells from old grid
            const CellB& cell00 = oldCells[srcY0 * oldWidth + srcX0];
            const CellB& cell10 = oldCells[srcY0 * oldWidth + srcX1];
            const CellB& cell01 = oldCells[srcY1 * oldWidth + srcX0];
            const CellB& cell11 = oldCells[srcY1 * oldWidth + srcX1];
            
            // Create interpolated cell and add to new grid
            newCells.push_back(createInterpolatedCellB(cell00, cell10, cell01, cell11, fx, fy));
        }
    }
    
    assert(newCells.size() == newWidth * newHeight && "New cells size must match dimensions");
    return newCells;
}

std::vector<Cell> WorldInterpolationTool::generateInterpolatedCellsA(
    const std::vector<Cell>& oldCells,
    uint32_t oldWidth, uint32_t oldHeight,
    uint32_t newWidth, uint32_t newHeight)
{
    assert(!oldCells.empty() && "Old cells vector must not be empty");
    assert(oldCells.size() == oldWidth * oldHeight && "Old cells size must match dimensions");
    assert(newWidth > 0 && newHeight > 0 && "New dimensions must be positive");
    
    std::vector<Cell> newCells;
    newCells.reserve(newWidth * newHeight);
    
    // Calculate scaling factors
    double scaleX = static_cast<double>(oldWidth) / static_cast<double>(newWidth);
    double scaleY = static_cast<double>(oldHeight) / static_cast<double>(newHeight);
    
    spdlog::debug("Interpolating Cell grid: {}x{} -> {}x{}, scale factors: {:.3f}x{:.3f}", 
                  oldWidth, oldHeight, newWidth, newHeight, scaleX, scaleY);
    
    // Generate interpolated cells for each position in the new grid
    for (uint32_t newY = 0; newY < newHeight; ++newY) {
        for (uint32_t newX = 0; newX < newWidth; ++newX) {
            // Map destination cell to source coordinates
            double srcX = (static_cast<double>(newX) + 0.5) * scaleX - 0.5;
            double srcY = (static_cast<double>(newY) + 0.5) * scaleY - 0.5;
            
            // Get integer source coordinates and fractional parts
            int srcX0 = static_cast<int>(std::floor(srcX));
            int srcY0 = static_cast<int>(std::floor(srcY));
            int srcX1 = srcX0 + 1;
            int srcY1 = srcY0 + 1;
            
            double fx = srcX - srcX0;
            double fy = srcY - srcY0;
            
            // Clamp to valid grid bounds
            clampToGrid(srcX0, srcY0, oldWidth, oldHeight);
            clampToGrid(srcX1, srcY1, oldWidth, oldHeight);
            
            // Get the 4 neighboring cells from old grid
            const Cell& cell00 = oldCells[srcY0 * oldWidth + srcX0];
            const Cell& cell10 = oldCells[srcY0 * oldWidth + srcX1];
            const Cell& cell01 = oldCells[srcY1 * oldWidth + srcX0];
            const Cell& cell11 = oldCells[srcY1 * oldWidth + srcX1];
            
            // Create interpolated cell and add to new grid
            newCells.push_back(createInterpolatedCell(cell00, cell10, cell01, cell11, fx, fy));
        }
    }
    
    assert(newCells.size() == newWidth * newHeight && "New cells size must match dimensions");
    return newCells;
}

// =================================================================
// BILINEAR INTERPOLATION HELPERS
// =================================================================

double WorldInterpolationTool::bilinearInterpolateDouble(double val00, double val10, 
                                                        double val01, double val11, 
                                                        double fx, double fy)
{
    // Standard bilinear interpolation formula
    return val00 * (1.0 - fx) * (1.0 - fy) +
           val10 * fx * (1.0 - fy) +
           val01 * (1.0 - fx) * fy +
           val11 * fx * fy;
}

Vector2d WorldInterpolationTool::bilinearInterpolateVector2d(const Vector2d& val00, const Vector2d& val10,
                                                           const Vector2d& val01, const Vector2d& val11,
                                                           double fx, double fy)
{
    // Interpolate x and y components separately
    double x = bilinearInterpolateDouble(val00.x, val10.x, val01.x, val11.x, fx, fy);
    double y = bilinearInterpolateDouble(val00.y, val10.y, val01.y, val11.y, fx, fy);
    return Vector2d(x, y);
}

// =================================================================
// WORLDB (PURE MATERIALS) INTERPOLATION
// =================================================================

MaterialType WorldInterpolationTool::interpolateMaterialType(const CellB& cell00, const CellB& cell10,
                                                           const CellB& cell01, const CellB& cell11,
                                                           double fx, double fy)
{
    // Calculate weighted contributions for each material type
    // Weight = fill_ratio * interpolation_weight
    double weight00 = cell00.getFillRatio() * (1.0 - fx) * (1.0 - fy);
    double weight10 = cell10.getFillRatio() * fx * (1.0 - fy);
    double weight01 = cell01.getFillRatio() * (1.0 - fx) * fy;
    double weight11 = cell11.getFillRatio() * fx * fy;
    
    // Find the material with highest weighted contribution
    MaterialType dominantMaterial = MaterialType::AIR;
    double maxWeight = 0.0;
    
    // Check cell00
    if (weight00 > maxWeight) {
        maxWeight = weight00;
        dominantMaterial = cell00.getMaterialType();
    }
    
    // Check cell10
    if (weight10 > maxWeight) {
        maxWeight = weight10;
        dominantMaterial = cell10.getMaterialType();
    }
    
    // Check cell01
    if (weight01 > maxWeight) {
        maxWeight = weight01;
        dominantMaterial = cell01.getMaterialType();
    }
    
    // Check cell11
    if (weight11 > maxWeight) {
        maxWeight = weight11;
        dominantMaterial = cell11.getMaterialType();
    }
    
    return dominantMaterial;
}

CellB WorldInterpolationTool::createInterpolatedCellB(const CellB& cell00, const CellB& cell10,
                                                     const CellB& cell01, const CellB& cell11,
                                                     double fx, double fy)
{
    // Interpolate material type (choose dominant)
    MaterialType materialType = interpolateMaterialType(cell00, cell10, cell01, cell11, fx, fy);
    
    // Interpolate fill ratio
    double fillRatio = bilinearInterpolateDouble(
        cell00.getFillRatio(), cell10.getFillRatio(),
        cell01.getFillRatio(), cell11.getFillRatio(),
        fx, fy);
    
    // Interpolate center of mass
    Vector2d com = bilinearInterpolateVector2d(
        cell00.getCOM(), cell10.getCOM(),
        cell01.getCOM(), cell11.getCOM(),
        fx, fy);
    
    // Interpolate velocity
    Vector2d velocity = bilinearInterpolateVector2d(
        cell00.getVelocity(), cell10.getVelocity(),
        cell01.getVelocity(), cell11.getVelocity(),
        fx, fy);
    
    // Create the interpolated cell
    CellB result(materialType, std::max(0.0, std::min(1.0, fillRatio)));
    result.setCOM(com);
    result.setVelocity(velocity);
    
    return result;
}

// resizeWorldB method removed - use generateInterpolatedCellsB instead

// =================================================================
// WORLDA (MIXED MATERIALS) INTERPOLATION
// =================================================================

Cell WorldInterpolationTool::createInterpolatedCell(const Cell& cell00, const Cell& cell10,
                                                   const Cell& cell01, const Cell& cell11,
                                                   double fx, double fy)
{
    Cell result;
    
    // Interpolate each material amount
    result.dirt = bilinearInterpolateDouble(cell00.dirt, cell10.dirt, cell01.dirt, cell11.dirt, fx, fy);
    result.water = bilinearInterpolateDouble(cell00.water, cell10.water, cell01.water, cell11.water, fx, fy);
    result.wood = bilinearInterpolateDouble(cell00.wood, cell10.wood, cell01.wood, cell11.wood, fx, fy);
    result.leaf = bilinearInterpolateDouble(cell00.leaf, cell10.leaf, cell01.leaf, cell11.leaf, fx, fy);
    result.metal = bilinearInterpolateDouble(cell00.metal, cell10.metal, cell01.metal, cell11.metal, fx, fy);
    
    // Clamp material amounts to valid range [0,1.1] (allowing slight overfill like original)
    result.dirt = std::max(0.0, std::min(1.1, result.dirt));
    result.water = std::max(0.0, std::min(1.1, result.water));
    result.wood = std::max(0.0, std::min(1.1, result.wood));
    result.leaf = std::max(0.0, std::min(1.1, result.leaf));
    result.metal = std::max(0.0, std::min(1.1, result.metal));
    
    // Interpolate center of mass
    result.com = bilinearInterpolateVector2d(cell00.com, cell10.com, cell01.com, cell11.com, fx, fy);
    
    // Interpolate velocity
    result.v = bilinearInterpolateVector2d(cell00.v, cell10.v, cell01.v, cell11.v, fx, fy);
    
    // Interpolate pressure (Vector2d for WorldA)
    result.pressure = bilinearInterpolateVector2d(cell00.pressure, cell10.pressure, cell01.pressure, cell11.pressure, fx, fy);
    
    return result;
}

// resizeWorldA method removed - use generateInterpolatedCellsA instead

// =================================================================
// UTILITY HELPERS
// =================================================================

void WorldInterpolationTool::clampToGrid(int& x, int& y, uint32_t width, uint32_t height)
{
    x = std::max(0, std::min(static_cast<int>(width - 1), x));
    y = std::max(0, std::min(static_cast<int>(height - 1), y));
}
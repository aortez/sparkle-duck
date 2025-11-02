#include "WorldInterpolationTool.h"
#include "Cell.h"
#include "Cell.h"
#include "MaterialType.h"
#include "World.h"

#include <algorithm>
#include <cassert>
#include <spdlog/spdlog.h>
#include <stdexcept>

// =================================================================
// PUBLIC INTERFACE.
// =================================================================

bool WorldInterpolationTool::resizeWorldWithBilinearFiltering(
    World& world, uint32_t newWidth, uint32_t newHeight)
{
    // This method is now deprecated - worlds should call resizeGrid directly.
    // which will use the generateInterpolatedCells* methods.
    spdlog::warn(
        "resizeWorldWithBilinearFiltering is deprecated - use world.resizeGrid() directly");
    world.resizeGrid(newWidth, newHeight);
    return true;
}

std::vector<Cell> WorldInterpolationTool::generateInterpolatedCellsB(
    const std::vector<Cell>& oldCells,
    uint32_t oldWidth,
    uint32_t oldHeight,
    uint32_t newWidth,
    uint32_t newHeight)
{
    assert(!oldCells.empty() && "Old cells vector must not be empty");
    assert(oldCells.size() == oldWidth * oldHeight && "Old cells size must match dimensions");
    assert(newWidth > 0 && newHeight > 0 && "New dimensions must be positive");

    std::vector<Cell> newCells;
    newCells.reserve(newWidth * newHeight);

    // Calculate scaling factors.
    double scaleX = static_cast<double>(oldWidth) / static_cast<double>(newWidth);
    double scaleY = static_cast<double>(oldHeight) / static_cast<double>(newHeight);

    spdlog::debug(
        "Interpolating Cell grid: {}x{} -> {}x{}, scale factors: {:.3f}x{:.3f}",
        oldWidth,
        oldHeight,
        newWidth,
        newHeight,
        scaleX,
        scaleY);

    // Generate interpolated cells for each position in the new grid.
    for (uint32_t newY = 0; newY < newHeight; ++newY) {
        for (uint32_t newX = 0; newX < newWidth; ++newX) {
            // Map destination cell to source coordinates.
            double srcX = (static_cast<double>(newX) + 0.5) * scaleX - 0.5;
            double srcY = (static_cast<double>(newY) + 0.5) * scaleY - 0.5;

            // Get integer source coordinates and fractional parts.
            int srcX0 = static_cast<int>(std::floor(srcX));
            int srcY0 = static_cast<int>(std::floor(srcY));
            int srcX1 = srcX0 + 1;
            int srcY1 = srcY0 + 1;

            double fx = srcX - srcX0;
            double fy = srcY - srcY0;

            // Clamp to valid grid bounds.
            clampToGrid(srcX0, srcY0, oldWidth, oldHeight);
            clampToGrid(srcX1, srcY1, oldWidth, oldHeight);

            // Get the 4 neighboring cells from old grid.
            const Cell& cell00 = oldCells[srcY0 * oldWidth + srcX0];
            const Cell& cell10 = oldCells[srcY0 * oldWidth + srcX1];
            const Cell& cell01 = oldCells[srcY1 * oldWidth + srcX0];
            const Cell& cell11 = oldCells[srcY1 * oldWidth + srcX1];

            // Create interpolated cell and add to new grid.
            newCells.push_back(createInterpolatedCellB(cell00, cell10, cell01, cell11, fx, fy));
        }
    }

    assert(newCells.size() == newWidth * newHeight && "New cells size must match dimensions");
    return newCells;
}

Vector2d WorldInterpolationTool::bilinearInterpolateVector2d(
    const Vector2d& val00,
    const Vector2d& val10,
    const Vector2d& val01,
    const Vector2d& val11,
    double fx,
    double fy)
{
    // Interpolate x and y components separately.
    double x = bilinearInterpolateDouble(val00.x, val10.x, val01.x, val11.x, fx, fy);
    double y = bilinearInterpolateDouble(val00.y, val10.y, val01.y, val11.y, fx, fy);
    return Vector2d{x, y};
}

double WorldInterpolationTool::bilinearInterpolateDouble(
    double val00, double val10, double val01, double val11, double fx, double fy)
{
    // Standard bilinear interpolation formula.
    return val00 * (1.0 - fx) * (1.0 - fy) + val10 * fx * (1.0 - fy) + val01 * (1.0 - fx) * fy
        + val11 * fx * fy;
}

// =================================================================
// WORLD (PURE MATERIALS) INTERPOLATION.
// =================================================================

MaterialType WorldInterpolationTool::interpolateMaterialType(
    MaterialType m00,
    MaterialType m10,
    MaterialType m01,
    MaterialType m11,
    double fx,
    double fy)
{
    // Simple bilinear interpolation - choose based on position.
    // Pick the material from the nearest corner.
    if (fx < 0.5 && fy < 0.5) return m00;
    if (fx >= 0.5 && fy < 0.5) return m10;
    if (fx < 0.5 && fy >= 0.5) return m01;
    return m11;
}

Cell WorldInterpolationTool::createInterpolatedCellB(
    const Cell& cell00,
    const Cell& cell10,
    const Cell& cell01,
    const Cell& cell11,
    double fx,
    double fy)
{
    // Interpolate material type (choose dominant).
    MaterialType materialType = interpolateMaterialType(
        cell00.getMaterialType(), cell10.getMaterialType(),
        cell01.getMaterialType(), cell11.getMaterialType(), fx, fy);

    // Interpolate fill ratio.
    double fillRatio = bilinearInterpolateDouble(
        cell00.getFillRatio(),
        cell10.getFillRatio(),
        cell01.getFillRatio(),
        cell11.getFillRatio(),
        fx,
        fy);

    // Interpolate center of mass.
    Vector2d com = bilinearInterpolateVector2d(
        cell00.getCOM(), cell10.getCOM(), cell01.getCOM(), cell11.getCOM(), fx, fy);

    // Interpolate velocity.
    Vector2d velocity = bilinearInterpolateVector2d(
        cell00.getVelocity(),
        cell10.getVelocity(),
        cell01.getVelocity(),
        cell11.getVelocity(),
        fx,
        fy);

    // Create the interpolated cell.
    Cell result(materialType, std::max(0.0, std::min(1.0, fillRatio)));
    result.setCOM(com);
    result.setVelocity(velocity);

    return result;
}

// resizeWorldB method removed - use generateInterpolatedCellsB instead.

// =================================================================
// WORLDA (MIXED MATERIALS) INTERPOLATION.
// =================================================================

// =================================================================

void WorldInterpolationTool::clampToGrid(int& x, int& y, uint32_t width, uint32_t height)
{
    x = std::max(0, std::min(static_cast<int>(width - 1), x));
    y = std::max(0, std::min(static_cast<int>(height - 1), y));
}
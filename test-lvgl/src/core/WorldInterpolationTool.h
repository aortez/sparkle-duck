#pragma once

#include "MaterialType.h"
#include "Vector2d.h"
#include <cstdint>
#include <vector>

namespace DirtSim {

// Forward declarations
class Cell;
class World;

/**
 * @brief Tool for bilinear interpolation-based world rescaling
 *
 * This tool provides bilinear filtering capabilities for rescaling simulation grids
 * while preserving material distribution, physics properties, and visual continuity.
 */
class WorldInterpolationTool {
public:
    // =================================================================
    // PUBLIC INTERFACE
    // =================================================================

    /**
     * @brief Resize a world using bilinear interpolation
     * @param world The world to resize
     * @param newWidth Target grid width
     * @param newHeight Target grid height
     * @return true if resize was successful, false otherwise
     */
    static bool resizeWorldWithBilinearFiltering(
        World& world, uint32_t newWidth, uint32_t newHeight);

    /**
     * @brief Generate interpolated cells for World without modifying the world
     * @param oldCells The current cell data
     * @param oldWidth Current grid width
     * @param oldHeight Current grid height
     * @param newWidth Target grid width
     * @param newHeight Target grid height
     * @return Vector of interpolated cells for the new grid dimensions
     */
    static std::vector<Cell> generateInterpolatedCellsB(
        const std::vector<Cell>& oldCells,
        uint32_t oldWidth,
        uint32_t oldHeight,
        uint32_t newWidth,
        uint32_t newHeight);

private:
    // Helper methods for bilinear interpolation.
    static double bilinearInterpolateDouble(
        double v00, double v10, double v01, double v11, double fx, double fy);

    static Vector2d bilinearInterpolateVector2d(
        const Vector2d& v00,
        const Vector2d& v10,
        const Vector2d& v01,
        const Vector2d& v11,
        double fx,
        double fy);

    static MaterialType interpolateMaterialType(
        MaterialType m00,
        MaterialType m10,
        MaterialType m01,
        MaterialType m11,
        double fx,
        double fy);

    static Cell createInterpolatedCellB(
        const Cell& cell00,
        const Cell& cell10,
        const Cell& cell01,
        const Cell& cell11,
        double fx,
        double fy);

    static void clampToGrid(int& x, int& y, uint32_t width, uint32_t height);
};

} // namespace DirtSim

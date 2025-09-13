#pragma once

#include "MaterialType.h"
#include "Vector2d.h"
#include <cstdint>
#include <vector>

// Forward declarations
class CellB;
class Cell;
class WorldInterface;

/**
 * @brief Tool for bilinear interpolation-based world rescaling
 *
 * This tool provides bilinear filtering capabilities for rescaling simulation grids
 * while preserving material distribution, physics properties, and visual continuity.
 * Supports both WorldA (mixed materials) and WorldB (pure materials) systems.
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
        WorldInterface& world, uint32_t newWidth, uint32_t newHeight);

    /**
     * @brief Generate interpolated cells for WorldB without modifying the world
     * @param oldCells The current cell data
     * @param oldWidth Current grid width
     * @param oldHeight Current grid height
     * @param newWidth Target grid width
     * @param newHeight Target grid height
     * @return Vector of interpolated cells for the new grid dimensions
     */
    static std::vector<CellB> generateInterpolatedCellsB(
        const std::vector<CellB>& oldCells,
        uint32_t oldWidth,
        uint32_t oldHeight,
        uint32_t newWidth,
        uint32_t newHeight);

    /**
     * @brief Generate interpolated cells for WorldA without modifying the world
     * @param oldCells The current cell data
     * @param oldWidth Current grid width
     * @param oldHeight Current grid height
     * @param newWidth Target grid width
     * @param newHeight Target grid height
     * @return Vector of interpolated cells for the new grid dimensions
     */
    static std::vector<Cell> generateInterpolatedCellsA(
        const std::vector<Cell>& oldCells,
        uint32_t oldWidth,
        uint32_t oldHeight,
        uint32_t newWidth,
        uint32_t newHeight);

private:
    // =================================================================
    // BILINEAR INTERPOLATION HELPERS
    // =================================================================

    /**
     * @brief Bilinear interpolation for scalar values
     */
    static double bilinearInterpolateDouble(
        double val00, double val10, double val01, double val11, double fx, double fy);

    /**
     * @brief Bilinear interpolation for Vector2d values
     */
    static Vector2d bilinearInterpolateVector2d(
        const Vector2d& val00,
        const Vector2d& val10,
        const Vector2d& val01,
        const Vector2d& val11,
        double fx,
        double fy);

    // =================================================================
    // WORLDB (PURE MATERIALS) INTERPOLATION
    // =================================================================

    /**
     * @brief Interpolate MaterialType by choosing dominant material weighted by fill ratio
     */
    static MaterialType interpolateMaterialType(
        const CellB& cell00,
        const CellB& cell10,
        const CellB& cell01,
        const CellB& cell11,
        double fx,
        double fy);

    /**
     * @brief Create an interpolated CellB from 4 neighboring cells
     */
    static CellB createInterpolatedCellB(
        const CellB& cell00,
        const CellB& cell10,
        const CellB& cell01,
        const CellB& cell11,
        double fx,
        double fy);

    // =================================================================
    // WORLDA (MIXED MATERIALS) INTERPOLATION
    // =================================================================

    /**
     * @brief Create an interpolated Cell from 4 neighboring cells
     */
    static Cell createInterpolatedCell(
        const Cell& cell00,
        const Cell& cell10,
        const Cell& cell01,
        const Cell& cell11,
        double fx,
        double fy);

    // =================================================================
    // SAMPLING HELPERS
    // =================================================================

    /**
     * @brief Get 4 neighboring cells for bilinear interpolation with boundary handling
     * @param world Source world
     * @param srcX Source x coordinate (can be fractional)
     * @param srcY Source y coordinate (can be fractional)
     * @param cell00 Output: bottom-left cell
     * @param cell10 Output: bottom-right cell
     * @param cell01 Output: top-left cell
     * @param cell11 Output: top-right cell
     * @param fx Output: fractional x weight [0,1]
     * @param fy Output: fractional y weight [0,1]
     * @return true if sampling is valid
     */
    template <typename CellType>
    static bool sampleNeighboringCells(
        const WorldInterface& world,
        double srcX,
        double srcY,
        CellType& cell00,
        CellType& cell10,
        CellType& cell01,
        CellType& cell11,
        double& fx,
        double& fy);

    /**
     * @brief Clamp coordinates to valid grid bounds
     */
    static void clampToGrid(int& x, int& y, uint32_t width, uint32_t height);
};
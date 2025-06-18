#pragma once

#include <string>
#include <cstdint>

// Forward declarations
class CellB;

/**
 * @brief Generates ASCII diagrams from WorldB grid state
 * 
 * This class provides utilities for converting WorldB grid data into
 * ASCII text representations for debugging, testing, and visualization.
 */
class WorldDiagramGenerator {
public:
    /**
     * @brief Generate ASCII diagram from a grid of CellB objects
     * 
     * Creates a bordered ASCII representation where each cell is represented
     * by 2 characters with spaces between cells. The format follows:
     * 
     * +------+
     * |DD WW |
     * |WW    |
     * +------+
     * 
     * @param cells Pointer to the first cell in a contiguous grid array
     * @param width Number of cells horizontally 
     * @param height Number of cells vertically
     * @return ASCII diagram string with borders and cell representations
     */
    static std::string generateAsciiDiagram(const CellB* cells, uint32_t width, uint32_t height);

private:
    /**
     * @brief Get cell at specific coordinates from flat array
     * @param cells Pointer to grid array
     * @param x Column coordinate
     * @param y Row coordinate  
     * @param width Grid width for index calculation
     * @return Reference to cell at (x,y)
     */
    static const CellB& getCellAt(const CellB* cells, uint32_t x, uint32_t y, uint32_t width);
};
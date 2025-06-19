#pragma once

#include <cstdint>
#include <string>

// Forward declarations
class WorldInterface;

/**
 * @brief Generates ASCII diagrams from world grid state
 *
 * This class provides utilities for converting world grid data into
 * ASCII text representations for debugging, testing, and visualization.
 */
class WorldDiagramGenerator {
public:
    /**
     * @brief Generate ASCII diagram from a world object
     *
     * Creates a bordered ASCII representation where each cell is represented
     * by 2 characters with spaces between cells. The format follows:
     *
     * +------+
     * |DD WW |
     * |WW    |
     * +------+
     *
     * @param world World interface to generate diagram from
     * @return ASCII diagram string with borders and cell representations
     */
    static std::string generateAsciiDiagram(const WorldInterface& world);
};

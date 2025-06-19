#include "WorldDiagramGenerator.h"
#include "CellInterface.h"
#include "WorldInterface.h"

#include <sstream>

std::string WorldDiagramGenerator::generateAsciiDiagram(const WorldInterface& world)
{
    std::ostringstream diagram;

    uint32_t width = world.getWidth();
    uint32_t height = world.getHeight();

    // Add top border (2 chars per cell + 1 space between each cell, except last)
    diagram << "+";
    for (uint32_t x = 0; x < width; ++x) {
        diagram << "--";
        if (x < width - 1) {
            diagram << "-"; // Space between cells
        }
    }
    diagram << "+\n";

    // Iterate through each row from top to bottom
    for (uint32_t y = 0; y < height; ++y) {
        diagram << "|"; // Left border

        // Add each cell's ASCII representation (2 characters each) with spaces between
        for (uint32_t x = 0; x < width; ++x) {
            diagram << world.getCellInterface(x, y).toAsciiCharacter();
            if (x < width - 1) {
                diagram << " "; // Space between cells
            }
        }

        diagram << "|\n"; // Right border and newline
    }

    // Add bottom border (2 chars per cell + 1 space between each cell, except last)
    diagram << "+";
    for (uint32_t x = 0; x < width; ++x) {
        diagram << "--";
        if (x < width - 1) {
            diagram << "-"; // Space between cells
        }
    }
    diagram << "+\n";

    return diagram.str();
}

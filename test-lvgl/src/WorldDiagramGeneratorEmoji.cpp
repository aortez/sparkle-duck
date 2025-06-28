#include "WorldDiagramGeneratorEmoji.h"
#include "Cell.h"
#include "CellB.h"
#include "CellInterface.h"
#include "MaterialType.h"
#include "WorldInterface.h"

#include <cmath>
#include <sstream>

std::string WorldDiagramGeneratorEmoji::generateEmojiDiagram(const WorldInterface& world)
{
    std::ostringstream diagram;

    uint32_t width = world.getWidth();
    uint32_t height = world.getHeight();

    // Top border with sparkles!
    diagram << "✨";
    for (uint32_t x = 0; x < width; ++x) {
        diagram << "━━";
    }
    diagram << "✨\n";

    // Each row.
    for (uint32_t y = 0; y < height; ++y) {
        diagram << "┃";

        for (uint32_t x = 0; x < width; ++x) {
            const auto& cell = world.getCellInterface(x, y);

            if (cell.isEmpty()) {
                diagram << "⬜";
            }
            else {
                // Get material type and fill ratio.
                auto cellB = dynamic_cast<const CellB*>(&cell);
                if (cellB) {
                    switch (cellB->getMaterialType()) {
                        case MaterialType::AIR:
                            diagram << "⬜";
                            break;
                        case MaterialType::DIRT:
                            diagram << "🟫";
                            break;
                        case MaterialType::WATER:
                            diagram << "💧";
                            break;
                        case MaterialType::WOOD:
                            diagram << "🪵";
                            break;
                        case MaterialType::SAND:
                            diagram << "🟨";
                            break;
                        case MaterialType::METAL:
                            diagram << "🔩";
                            break;
                        case MaterialType::LEAF:
                            diagram << "🍃";
                            break;
                        case MaterialType::WALL:
                            diagram << "🧱";
                            break;
                        default:
                            diagram << "❓";
                            break;
                    }
                }
                else {
                    // WorldA cells - check dirt/water content.
                    auto cellA = dynamic_cast<const Cell*>(&cell);
                    if (cellA) {
                        if (cellA->water > cellA->dirt) {
                            diagram << "💧";
                        }
                        else {
                            diagram << "🟫";
                        }
                    }
                }
            }

            if (x < width - 1) {
                diagram << " ";
            }
        }

        diagram << "┃\n";
    }

    // Bottom border.
    diagram << "✨";
    for (uint32_t x = 0; x < width; ++x) {
        diagram << "━━";
    }
    diagram << "✨\n";

    return diagram.str();
}

std::string WorldDiagramGeneratorEmoji::generateMixedDiagram(const WorldInterface& world)
{
    std::ostringstream diagram;

    uint32_t width = world.getWidth();
    uint32_t height = world.getHeight();

    // Top border.
    diagram << "🦆✨ Sparkle Duck World ✨🦆\n";
    diagram << "┌";
    for (uint32_t x = 0; x < width; ++x) {
        diagram << "───";
        if (x < width - 1) diagram << "┬";
    }
    diagram << "┐\n";

    // Each row.
    for (uint32_t y = 0; y < height; ++y) {
        diagram << "│";

        for (uint32_t x = 0; x < width; ++x) {
            const auto& cell = world.getCellInterface(x, y);

            if (cell.isEmpty()) {
                diagram << "   ";
            }
            else {
                // Get material type and fill ratio.
                auto cellB = dynamic_cast<const CellB*>(&cell);
                if (cellB) {
                    float fill = cellB->getFillRatio();

                    // Material emoji.
                    switch (cellB->getMaterialType()) {
                        case MaterialType::AIR:
                            diagram << " ";
                            break;
                        case MaterialType::DIRT:
                            diagram << "🟫";
                            break;
                        case MaterialType::WATER:
                            diagram << "💧";
                            break;
                        case MaterialType::WOOD:
                            diagram << "🪵";
                            break;
                        case MaterialType::SAND:
                            diagram << "🟨";
                            break;
                        case MaterialType::METAL:
                            diagram << "🔩";
                            break;
                        case MaterialType::LEAF:
                            diagram << "🍃";
                            break;
                        case MaterialType::WALL:
                            diagram << "🧱";
                            break;
                        default:
                            diagram << "❓";
                            break;
                    }

                    // Fill level indicator.
                    if (fill < 0.25) {
                        diagram << "░";
                    }
                    else if (fill < 0.5) {
                        diagram << "▒";
                    }
                    else if (fill < 0.75) {
                        diagram << "▓";
                    }
                    else {
                        diagram << "█";
                    }
                }
                else {
                    // WorldA fallback.
                    diagram << "? ";
                }
            }

            if (x < width - 1) {
                diagram << "│";
            }
        }

        diagram << "│\n";

        // Horizontal divider (except last row).
        if (y < height - 1) {
            diagram << "├";
            for (uint32_t x = 0; x < width; ++x) {
                diagram << "───";
                if (x < width - 1) {
                    diagram << "┼";
                }
            }
            diagram << "┤\n";
        }
    }

    // Bottom border.
    diagram << "└";
    for (uint32_t x = 0; x < width; ++x) {
        diagram << "───";
        if (x < width - 1) diagram << "┴";
    }
    diagram << "┘\n";

    return diagram.str();
}
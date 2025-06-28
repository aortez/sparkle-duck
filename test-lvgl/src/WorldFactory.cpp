#include "WorldFactory.h"
#include "World.h"
#include "WorldB.h"
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

std::unique_ptr<WorldInterface> createWorld(
    WorldType type, uint32_t width, uint32_t height, lv_obj_t* draw_area)
{
    switch (type) {
        case WorldType::RulesA: {
            // Create original World (mixed materials).
            auto world = std::make_unique<World>(width, height, draw_area);
            world->setup(); // Ensure clean initial state with proper setup.
            return world;
        }

        case WorldType::RulesB: {
            // Create WorldB (pure materials).
            auto worldB = std::make_unique<WorldB>(width, height, draw_area);

            // Apply default WorldB configuration.
            worldB->setWallsEnabled(false); // Default to walls disabled to match World behavior.
            worldB->setup();                // Ensure clean initial state.

            return worldB;
        }

        default:
            throw std::runtime_error(
                "Unknown WorldType: " + std::to_string(static_cast<int>(type)));
    }
}

const char* getWorldTypeName(WorldType type)
{
    switch (type) {
        case WorldType::RulesA:
            return "World (RulesA)";
        case WorldType::RulesB:
            return "WorldB (RulesB)";
        default:
            return "Unknown";
    }
}

WorldType parseWorldType(const std::string& typeStr)
{
    // Convert to lowercase for case-insensitive comparison.
    std::string lowerStr = typeStr;
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), [](unsigned char c) {
        return std::tolower(c);
    });

    if (lowerStr == "rulesa" || lowerStr == "rules_a" || lowerStr == "a" || lowerStr == "world") {
        return WorldType::RulesA;
    }
    else if (
        lowerStr == "rulesb" || lowerStr == "rules_b" || lowerStr == "b" || lowerStr == "worldb") {
        return WorldType::RulesB;
    }
    else {
        throw std::runtime_error(
            "Unknown world type string: '" + typeStr + "'. Valid options: rulesA, rulesB");
    }
}
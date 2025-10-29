#pragma once

#include "WorldInterface.h"
#include <cstdint>
#include <memory>
#include <string>

// Forward declarations
typedef struct _lv_obj_t lv_obj_t;

/**
 * WorldType enum for selecting which physics system to use
 */
enum class WorldType {
    RulesA, // Original World (mixed dirt/water materials)
    RulesB  // New WorldB (pure materials with fill ratios)
};

/**
 * Factory function to create worlds polymorphically through WorldInterface
 *
 * @param type The world type to create (RulesA or RulesB)
 * @param width Grid width in cells
 * @param height Grid height in cells
 * @return Unique pointer to WorldInterface implementation
 * @throws std::runtime_error if unknown world type is requested
 *
 * Note: Drawing is handled by passing lv_obj_t& to draw() method when rendering is needed.
 */
std::unique_ptr<WorldInterface> createWorld(WorldType type, uint32_t width, uint32_t height);

/**
 * Get string name for WorldType (useful for logging/debugging)
 */
const char* getWorldTypeName(WorldType type);

/**
 * Parse WorldType from string (useful for command-line parsing)
 *
 * @param typeStr String like "rulesA", "rulesB" (case insensitive)
 * @return WorldType enum value
 * @throws std::runtime_error if unknown type string
 */
WorldType parseWorldType(const std::string& typeStr);
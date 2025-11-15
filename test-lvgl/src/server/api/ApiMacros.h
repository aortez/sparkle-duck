#pragma once

#include "core/reflect.h"

namespace DirtSim {

/**
 * @brief Define API name marker type and cached name.
 *
 * Usage: DEFINE_API_NAME(SimRun) at the top of the API namespace.
 * Creates a marker struct and cached api_name using reflect::type_name.
 */
#define DEFINE_API_NAME(Name)                                           \
    struct Name {};                                                     \
    inline static constexpr auto api_name = reflect::type_name<Name>(); \
    static_assert(!api_name.empty(), "API name must not be empty");     \
    static_assert(api_name.size() > 0, "API name extraction failed")

/**
 * @brief Add name() method to Command or Okay structs.
 *
 * Usage: API_COMMAND_NAME() inside Command/Okay struct definitions.
 * Returns the cached api_name from the namespace.
 */
#define API_COMMAND_NAME()                   \
    static constexpr std::string_view name() \
    {                                        \
        return api_name;                     \
    }

} // namespace DirtSim

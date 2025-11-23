#pragma once

// This file aggregates all server state definitions.
// Each state has its own header file for better organization.

#include "Idle.h"
#include "Shutdown.h"
#include "SimPaused.h"
#include "SimRunning.h"
#include "Startup.h"
#include "StateForward.h"
#include "core/World.h" // Must be before SimRunning.h for complete type in unique_ptr.

namespace DirtSim {
namespace Server {
namespace State {

/**
 * @brief Forward-declarable State wrapper class.
 *
 * Wraps the state variant to enable forward declaration,
 * reducing compilation dependencies.
 *
 * Defined here (not in StateForward.h) because variant requires complete types.
 */
class Any {
public:
    using Variant = std::variant<Startup, Idle, SimRunning, SimPaused, Shutdown>;

    // Constructor from any state type.
    template <typename T>
    Any(T&& state) : variant_(std::forward<T>(state))
    {}

    // Default constructor.
    Any() = default;

    // Accessor for the underlying variant.
    Variant& getVariant() { return variant_; }
    const Variant& getVariant() const { return variant_; }

private:
    Variant variant_;
};

/**
 * @brief Get the name of the current state.
 * Requires complete state definitions, so defined here after all includes.
 */
inline std::string getCurrentStateName(const Any& state)
{
    return std::visit([](const auto& s) { return std::string(s.name()); }, state.getVariant());
}

} // namespace State
} // namespace Server
} // namespace DirtSim

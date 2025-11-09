#pragma once

#include "api/DrawDebugToggle.h"
#include "api/Exit.h"
#include "api/MouseDown.h"
#include "api/MouseMove.h"
#include "api/MouseUp.h"
#include "api/Screenshot.h"
#include "api/SimPause.h"
#include "api/SimRun.h"
#include "core/api/UiUpdateEvent.h"
#include <concepts>
#include <string>
#include <variant>

namespace DirtSim {
namespace Ui {

/**
 * @brief Event definitions for the UI state machine.
 *
 * Events include lifecycle, server connection, and API commands.
 * Mouse events are API commands - both local (from LVGL) and remote (from WebSocket)
 * use the same API, ensuring consistent behavior.
 */

// =================================================================
// EVENT NAME CONCEPT
// =================================================================

/**
 * @brief Concept for events that have a name() method.
 */
template <typename T>
concept HasEventName = requires {
    { T::name() } -> std::convertible_to<const char*>;
};

// =================================================================
// LIFECYCLE EVENTS
// =================================================================

/**
 * @brief Initialization complete.
 */
struct InitCompleteEvent {
    static constexpr const char* name() { return "InitCompleteEvent"; }
};

// =================================================================
// SERVER CONNECTION EVENTS
// =================================================================

/**
 * @brief Connect to DSSM server.
 */
struct ConnectToServerCommand {
    std::string host;
    uint16_t port;
    static constexpr const char* name() { return "ConnectToServerCommand"; }
};

/**
 * @brief Server connection established.
 */
struct ServerConnectedEvent {
    static constexpr const char* name() { return "ServerConnectedEvent"; }
};

/**
 * @brief Server connection lost.
 */
struct ServerDisconnectedEvent {
    std::string reason;
    static constexpr const char* name() { return "ServerDisconnectedEvent"; }
};

/**
 * @brief Request world update from DSSM server.
 */
struct RequestWorldUpdateCommand {
    static constexpr const char* name() { return "RequestWorldUpdateCommand"; }
};

/**
 * @brief Frame ready notification from DSSM server.
 */
struct FrameReadyNotification {
    uint64_t stepNumber;
    int64_t timestamp;
    static constexpr const char* name() { return "FrameReadyNotification"; }
};

/**
 * @brief Server confirmed it's running (response to sim_run command).
 */
struct ServerRunningConfirmedEvent {
    static constexpr const char* name() { return "ServerRunningConfirmedEvent"; }
};

// =================================================================
// EVENT VARIANT
// =================================================================

/**
 * @brief Variant containing all UI event types.
 */
using Event = std::variant<
    // Lifecycle
    InitCompleteEvent,

    // Server connection
    ConnectToServerCommand,
    ServerConnectedEvent,
    ServerDisconnectedEvent,
    ServerRunningConfirmedEvent,
    RequestWorldUpdateCommand,
    FrameReadyNotification,

    // World updates from DSSM server
    DirtSim::UiUpdateEvent,

    // API commands (local from LVGL or remote from WebSocket)
    DirtSim::UiApi::DrawDebugToggle::Cwc,
    DirtSim::UiApi::Exit::Cwc,
    DirtSim::UiApi::MouseDown::Cwc,
    DirtSim::UiApi::MouseMove::Cwc,
    DirtSim::UiApi::MouseUp::Cwc,
    DirtSim::UiApi::Screenshot::Cwc,
    DirtSim::UiApi::SimPause::Cwc,
    DirtSim::UiApi::SimRun::Cwc
>;

/**
 * @brief Helper to get event name from variant.
 */
inline std::string getEventName(const Event& event)
{
    return std::visit([](auto&& e) { return std::string(e.name()); }, event);
}

} // namespace Ui
} // namespace DirtSim

#include "CommandDispatcher.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Client {

CommandDispatcher::CommandDispatcher()
{
    spdlog::debug(
        "CommandDispatcher: Registering server API commands with response deserializers...");

    // Explicitly register each command with both Command and Okay types.
    // This allows full response deserialization using the message_type metadata.
    registerCommand<Api::CellGet::Command, Api::CellGet::Okay>();
    registerCommand<Api::CellSet::Command, std::monostate>();
    registerCommand<Api::DiagramGet::Command, Api::DiagramGet::Okay>();
    registerCommand<Api::Exit::Command, std::monostate>();
    registerCommand<Api::GravitySet::Command, std::monostate>();
    // registerCommand<Api::PeersGet::Command, Api::PeersGet::Okay>();  // TODO: PeerInfo needs JSON
    // serialization.
    registerCommand<Api::PerfStatsGet::Command, Api::PerfStatsGet::Okay>();
    registerCommand<Api::PhysicsSettingsGet::Command, Api::PhysicsSettingsGet::Okay>();
    registerCommand<Api::PhysicsSettingsSet::Command, std::monostate>();
    registerCommand<Api::RenderFormatGet::Command, Api::RenderFormatGet::Okay>();
    registerCommand<Api::RenderFormatSet::Command, Api::RenderFormatSet::Okay>();
    registerCommand<Api::Reset::Command, std::monostate>();
    registerCommand<Api::ScenarioConfigSet::Command, Api::ScenarioConfigSet::Okay>();
    registerCommand<Api::SeedAdd::Command, std::monostate>();
    registerCommand<Api::SimRun::Command, Api::SimRun::Okay>();
    registerCommand<Api::SpawnDirtBall::Command, std::monostate>();
    registerCommand<Api::StateGet::Command, Api::StateGet::Okay>();
    registerCommand<Api::StatusGet::Command, Api::StatusGet::Okay>();
    // registerCommand<Api::TimerStatsGet::Command, Api::TimerStatsGet::Okay>();  // TODO:
    // TimerEntry needs JSON serialization.
    registerCommand<Api::WorldResize::Command, std::monostate>();

    spdlog::info("CommandDispatcher: Registered {} commands", handlers_.size());
}

Result<std::string, ApiError> CommandDispatcher::dispatch(
    Network::WebSocketService& client, const std::string& commandName, const nlohmann::json& body)
{
    auto it = handlers_.find(commandName);
    if (it == handlers_.end()) {
        return Result<std::string, ApiError>::error(ApiError{ "Unknown command: " + commandName });
    }

    spdlog::debug("CommandDispatcher: Dispatching command '{}'", commandName);
    return it->second(client, body);
}

bool CommandDispatcher::hasCommand(const std::string& commandName) const
{
    return handlers_.find(commandName) != handlers_.end();
}

std::vector<std::string> CommandDispatcher::getCommandNames() const
{
    std::vector<std::string> names;
    names.reserve(handlers_.size());
    for (const auto& [name, handler] : handlers_) {
        names.push_back(name);
    }
    return names;
}

} // namespace Client
} // namespace DirtSim

#include "CommandDispatcher.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Client {

CommandDispatcher::CommandDispatcher()
{
    spdlog::debug("CommandDispatcher: Registering server API commands...");
    registerVariant(static_cast<ApiCommand*>(nullptr));

    spdlog::debug("CommandDispatcher: Registering UI API commands...");
    registerVariant(static_cast<Ui::UiApiCommand*>(nullptr));

    spdlog::info("CommandDispatcher: Registered {} commands", handlers_.size());
}

Result<std::string, ApiError> CommandDispatcher::dispatch(
    Network::WebSocketClient& client, const std::string& commandName, const nlohmann::json& body)
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

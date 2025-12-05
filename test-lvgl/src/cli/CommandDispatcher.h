#pragma once

#include "core/ReflectSerializer.h"
#include "core/network/WebSocketClient.h"
#include "server/api/ApiCommand.h"
#include "server/api/ApiError.h"
#include "ui/state-machine/api/UiApiCommand.h"
#include <functional>
#include <map>
#include <nlohmann/json.hpp>
#include <string>

namespace DirtSim {
namespace Client {

/**
 * @brief Generic command dispatcher for type-safe WebSocket command execution.
 *
 * Builds a runtime dispatch table from compile-time command types.
 * Automatically registers all commands from ApiCommand and UiApiCommand variants.
 */
class CommandDispatcher {
public:
    /**
     * @brief Handler function signature.
     *
     * Takes a WebSocketClient and JSON body, returns JSON response string.
     */
    using Handler = std::function<Result<std::string, ApiError>(
        Network::WebSocketClient&, const nlohmann::json&)>;

    /**
     * @brief Construct dispatcher and register all known command types.
     */
    CommandDispatcher();

    /**
     * @brief Dispatch command by name using type-safe execution.
     *
     * @param client Connected WebSocketClient.
     * @param commandName Command name (e.g., "StateGet", "StatusGet").
     * @param body JSON command body (can be empty for commands with no fields).
     * @return Result with JSON response string on success, ApiError on failure.
     */
    Result<std::string, ApiError> dispatch(
        Network::WebSocketClient& client,
        const std::string& commandName,
        const nlohmann::json& body);

    /**
     * @brief Check if a command name is registered.
     */
    bool hasCommand(const std::string& commandName) const;

    /**
     * @brief Get list of all registered command names.
     */
    std::vector<std::string> getCommandNames() const;

private:
    /**
     * @brief Register a single command type with type-safe handler.
     *
     * Creates a handler that:
     * 1. Deserializes JSON body to CommandT
     * 2. Calls client.sendCommand<CommandT>(cmd)
     * 3. Converts typed result back to JSON string
     */
    template <typename CommandT>
    void registerCommand()
    {
        std::string cmdName(CommandT::name());
        handlers_[cmdName] =
            [cmdName](Network::WebSocketClient& client, const nlohmann::json& body) {
                // Deserialize JSON body → typed command.
                CommandT cmd;
                if (!body.empty()) {
                    try {
                        cmd = CommandT::fromJson(body);
                    }
                    catch (const std::exception& e) {
                        return Result<std::string, ApiError>::error(
                            ApiError{ std::string("Failed to parse command body: ") + e.what() });
                    }
                }

                // Build command JSON with command name.
                nlohmann::json commandJson = cmd.toJson();
                commandJson["command"] = cmdName;

                // Send JSON and receive response.
                auto result = client.sendJsonAndReceive(commandJson.dump());
                if (result.isError()) {
                    return Result<std::string, ApiError>::error(result.errorValue());
                }

                // Response is already JSON string - return it.
                return Result<std::string, ApiError>::okay(result.value());
            };
    }

    /**
     * @brief Register all command types from a variant.
     *
     * Uses fold expression to call registerCommand<T>() for each type.
     */
    template <typename... CommandTypes>
    void registerVariant(std::variant<CommandTypes...>*)
    {
        (registerCommand<CommandTypes>(), ...);
    }

    std::map<std::string, Handler> handlers_;
};

} // namespace Client
} // namespace DirtSim

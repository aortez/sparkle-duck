#pragma once

#include "core/ReflectSerializer.h"
#include "core/network/WebSocketService.h"
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
        Network::WebSocketService&, const nlohmann::json&)>;

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
        Network::WebSocketService& client,
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
    /**
     * @brief Register command with both Command and Okay types for full response deserialization.
     */
    template <typename CommandT, typename OkayT>
    void registerCommand()
    {
        std::string cmdName(CommandT::name());
        handlers_[cmdName] = [cmdName](
                                 Network::WebSocketService& client, const nlohmann::json& body) {
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

            // Build binary envelope with command.
            static std::atomic<uint64_t> nextId{ 1 };
            uint64_t id = nextId.fetch_add(1);
            auto envelope = Network::make_command_envelope(id, cmd);

            // Send binary envelope and receive binary response.
            auto envelopeResult = client.sendBinaryAndReceive(envelope);
            if (envelopeResult.isError()) {
                return Result<std::string, ApiError>::error(
                    ApiError{ envelopeResult.errorValue() });
            }

            // Deserialize typed response from envelope.
            const auto& responseEnvelope = envelopeResult.value();
            try {
                auto result = Network::extract_result<OkayT, ApiError>(responseEnvelope);

                if (result.isError()) {
                    nlohmann::json errorJson;
                    errorJson["error"] = result.errorValue().message;
                    errorJson["id"] = responseEnvelope.id;
                    return Result<std::string, ApiError>::okay(errorJson.dump());
                }

                // Success - convert typed response to JSON for display.
                nlohmann::json resultJson;
                if constexpr (std::is_same_v<OkayT, std::monostate>) {
                    resultJson["success"] = true;
                }
                else {
                    try {
                        resultJson["value"] = ReflectSerializer::to_json(result.value());
                    }
                    catch (const std::exception& e) {
                        // Complex type that ReflectSerializer can't handle - show basic success.
                        spdlog::debug("Cannot serialize {} response: {}", cmdName, e.what());
                        resultJson["success"] = true;
                        resultJson["note"] = "Response received but not displayable (complex type)";
                    }
                }
                resultJson["id"] = responseEnvelope.id;
                return Result<std::string, ApiError>::okay(resultJson.dump());
            }
            catch (const std::exception& e) {
                return Result<std::string, ApiError>::error(
                    ApiError{ std::string("Failed to deserialize response: ") + e.what() });
            }
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

#pragma once

#include <cassert>
#include <functional>

/**
 * @brief Bundles a command with its response callback for async command handling.
 *
 * This template enforces type safety between Command and Response types and
 * prevents accidentally sending multiple responses to the same command.
 *
 * @tparam Command The command type containing command parameters.
 * @tparam Response The response type (typically Result<OkayType, ErrorType>).
 */
template <typename Command, typename Response>
struct CommandWithCallback {
    Command command;
    std::function<void(Response)> callback;

    /**
     * @brief Send a response by invoking the callback.
     * @param response The response to send (moved into callback).
     *
     * Asserts if called more than once to prevent double-send bugs.
     */
    void sendResponse(Response&& response) const
    {
        assert(!responseSent && "Response already sent!");
        if (callback) {
            callback(std::move(response));
            responseSent = true;
        }
    }

    /**
     * @brief Get event name for logging.
     * @return Generic name for API commands.
     */
    static constexpr const char* name() { return "ApiCommand"; }

    mutable bool responseSent = false;
};

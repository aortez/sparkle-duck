#pragma once

#include "core/Result.h"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <zpp_bits.h>

namespace DirtSim {
namespace Network {

/**
 * @brief Unified message envelope for binary protocol.
 *
 * Works for both commands (client→server) and responses (server→client).
 * The message_type field determines how to interpret the payload.
 *
 * For commands:
 *   - message_type = command name (e.g., "state_get", "sim_run")
 *   - payload = zpp_bits serialized Command struct
 *
 * For responses:
 *   - message_type = command name + "_response" (e.g., "state_get_response")
 *   - payload = zpp_bits serialized SerializableResult<OkayType, ApiError>
 */
struct MessageEnvelope {
    uint64_t id;                    // Correlation ID for request/response matching.
    std::string message_type;       // Message type identifier.
    std::vector<std::byte> payload; // zpp_bits serialized content.

    using serialize = zpp::bits::members<3>;
};

/**
 * @brief Serializable result type for binary protocol responses.
 *
 * Since Result<T,E> wraps std::expected which may not serialize directly,
 * we use this explicit struct with optional fields.
 *
 * Exactly one of value or error will be populated.
 */
template <typename T, typename E>
struct SerializableResult {
    std::optional<T> value; // Present if success.
    std::optional<E> error; // Present if failure.

    using serialize = zpp::bits::members<2>;

    /// Create from a Result.
    static SerializableResult from_result(const Result<T, E>& result)
    {
        SerializableResult sr;
        if (result.isValue()) {
            sr.value = result.value();
        }
        else {
            sr.error = result.errorValue();
        }
        return sr;
    }

    /// Convert to a Result.
    Result<T, E> to_result() const
    {
        if (value.has_value()) {
            return Result<T, E>::okay(value.value());
        }
        else if (error.has_value()) {
            return Result<T, E>::error(error.value());
        }
        else {
            // Should never happen if properly constructed.
            return Result<T, E>::error(E{});
        }
    }

    /// Check if this represents a success.
    bool is_value() const { return value.has_value(); }

    /// Check if this represents an error.
    bool is_error() const { return error.has_value(); }
};

// ============================================================================
// Helper functions for serializing/deserializing envelopes.
// ============================================================================

/**
 * @brief Serialize a MessageEnvelope to bytes.
 * @param envelope The envelope to serialize.
 * @return Serialized bytes.
 */
inline std::vector<std::byte> serialize_envelope(const MessageEnvelope& envelope)
{
    std::vector<std::byte> data;
    zpp::bits::out out(data);
    out(envelope).or_throw();
    return data;
}

/**
 * @brief Deserialize a MessageEnvelope from bytes.
 * @param data The bytes to deserialize.
 * @return Deserialized envelope.
 * @throws std::exception on deserialization failure.
 */
inline MessageEnvelope deserialize_envelope(const std::vector<std::byte>& data)
{
    MessageEnvelope envelope;
    zpp::bits::in in(data);
    in(envelope).or_throw();
    return envelope;
}

/**
 * @brief Serialize a payload (any zpp_bits-compatible type) to bytes.
 * @tparam T The type to serialize.
 * @param payload The payload to serialize.
 * @return Serialized bytes.
 */
template <typename T>
std::vector<std::byte> serialize_payload(const T& payload)
{
    std::vector<std::byte> data;
    zpp::bits::out out(data);
    out(payload).or_throw();
    return data;
}

/**
 * @brief Deserialize a payload from bytes.
 * @tparam T The type to deserialize to.
 * @param data The bytes to deserialize.
 * @return Deserialized payload.
 * @throws std::exception on deserialization failure.
 */
template <typename T>
T deserialize_payload(const std::vector<std::byte>& data)
{
    // Suppress false positive -Wmaybe-uninitialized with GCC + -O3.
    // zpp_bits initializes the payload, but the compiler can't prove it.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
    T payload;
    zpp::bits::in in(data);
    in(payload).or_throw();
    return payload;
#pragma GCC diagnostic pop
}

/**
 * @brief Create a command envelope ready to send.
 * @tparam CommandT The command type.
 * @param id Correlation ID.
 * @param cmd The command to send.
 * @return MessageEnvelope ready for serialization.
 */
template <typename CommandT>
MessageEnvelope make_command_envelope(uint64_t id, const CommandT& cmd)
{
    MessageEnvelope envelope;
    envelope.id = id;
    envelope.message_type = std::string(CommandT::name());
    envelope.payload = serialize_payload(cmd);
    return envelope;
}

/**
 * @brief Create a response envelope ready to send.
 * @tparam OkayT The success type.
 * @tparam ErrorT The error type.
 * @param id Correlation ID (should match the request).
 * @param command_name The original command name.
 * @param result The result to send.
 * @return MessageEnvelope ready for serialization.
 */
template <typename OkayT, typename ErrorT>
MessageEnvelope make_response_envelope(
    uint64_t id, const std::string& command_name, const Result<OkayT, ErrorT>& result)
{
    MessageEnvelope envelope;
    envelope.id = id;
    envelope.message_type = command_name + "_response";
    envelope.payload = serialize_payload(SerializableResult<OkayT, ErrorT>::from_result(result));
    return envelope;
}

/**
 * @brief Extract a result from a response envelope.
 * @tparam OkayT The expected success type.
 * @tparam ErrorT The expected error type.
 * @param envelope The response envelope.
 * @return The deserialized result.
 * @throws std::exception on deserialization failure.
 */
template <typename OkayT, typename ErrorT>
Result<OkayT, ErrorT> extract_result(const MessageEnvelope& envelope)
{
    auto sr = deserialize_payload<SerializableResult<OkayT, ErrorT>>(envelope.payload);
    return sr.to_result();
}

} // namespace Network
} // namespace DirtSim

#include "core/network/BinaryProtocol.h"
#include "server/api/ApiError.h"
#include <gtest/gtest.h>

using namespace DirtSim;
using namespace DirtSim::Network;

// ============================================================================
// MessageEnvelope Tests
// ============================================================================

TEST(BinaryProtocolTest, MessageEnvelopeRoundtrip)
{
    // Create an envelope.
    MessageEnvelope original;
    original.id = 12345;
    original.message_type = "state_get";
    original.payload = { std::byte{ 0x01 }, std::byte{ 0x02 }, std::byte{ 0x03 } };

    // Serialize.
    auto bytes = serialize_envelope(original);
    EXPECT_GT(bytes.size(), 0u);

    // Deserialize.
    auto deserialized = deserialize_envelope(bytes);

    // Verify.
    EXPECT_EQ(deserialized.id, original.id);
    EXPECT_EQ(deserialized.message_type, original.message_type);
    EXPECT_EQ(deserialized.payload, original.payload);
}

TEST(BinaryProtocolTest, MessageEnvelopeEmptyPayload)
{
    // Envelope with no payload (some commands have no parameters).
    MessageEnvelope original;
    original.id = 1;
    original.message_type = "exit";
    original.payload = {};

    auto bytes = serialize_envelope(original);
    auto deserialized = deserialize_envelope(bytes);

    EXPECT_EQ(deserialized.id, original.id);
    EXPECT_EQ(deserialized.message_type, original.message_type);
    EXPECT_TRUE(deserialized.payload.empty());
}

TEST(BinaryProtocolTest, MessageEnvelopeLongMessageType)
{
    // Test with a longer message type name.
    MessageEnvelope original;
    original.id = 999999999;
    original.message_type = "physics_settings_get_response";
    original.payload = { std::byte{ 0xFF } };

    auto bytes = serialize_envelope(original);
    auto deserialized = deserialize_envelope(bytes);

    EXPECT_EQ(deserialized.id, original.id);
    EXPECT_EQ(deserialized.message_type, original.message_type);
}

// ============================================================================
// SerializableResult Tests
// ============================================================================

// Simple test struct for result payload.
struct TestOkay {
    int value;
    std::string name;

    using serialize = zpp::bits::members<2>;

    bool operator==(const TestOkay& other) const
    {
        return value == other.value && name == other.name;
    }
};

TEST(BinaryProtocolTest, SerializableResultSuccessRoundtrip)
{
    // Create a success result.
    Result<TestOkay, ApiError> original = Result<TestOkay, ApiError>::okay(TestOkay{ 42, "test" });

    // Convert to serializable form.
    auto sr = SerializableResult<TestOkay, ApiError>::from_result(original);
    EXPECT_TRUE(sr.is_value());
    EXPECT_FALSE(sr.is_error());

    // Serialize.
    auto bytes = serialize_payload(sr);
    EXPECT_GT(bytes.size(), 0u);

    // Deserialize.
    auto deserialized = deserialize_payload<SerializableResult<TestOkay, ApiError>>(bytes);
    EXPECT_TRUE(deserialized.is_value());
    EXPECT_FALSE(deserialized.is_error());

    // Convert back to Result.
    auto result = deserialized.to_result();
    EXPECT_TRUE(result.isValue());
    EXPECT_EQ(result.value().value, 42);
    EXPECT_EQ(result.value().name, "test");
}

TEST(BinaryProtocolTest, SerializableResultErrorRoundtrip)
{
    // Create an error result.
    Result<TestOkay, ApiError> original =
        Result<TestOkay, ApiError>::error(ApiError{ "Something went wrong" });

    // Convert to serializable form.
    auto sr = SerializableResult<TestOkay, ApiError>::from_result(original);
    EXPECT_FALSE(sr.is_value());
    EXPECT_TRUE(sr.is_error());

    // Serialize.
    auto bytes = serialize_payload(sr);

    // Deserialize.
    auto deserialized = deserialize_payload<SerializableResult<TestOkay, ApiError>>(bytes);
    EXPECT_FALSE(deserialized.is_value());
    EXPECT_TRUE(deserialized.is_error());

    // Convert back to Result.
    auto result = deserialized.to_result();
    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.errorValue().message, "Something went wrong");
}

// ============================================================================
// Helper Function Tests
// ============================================================================

// Mock command for testing.
struct MockCommand {
    int param1;
    std::string param2;

    using serialize = zpp::bits::members<2>;

    static constexpr std::string_view name() { return "mock_command"; }
};

TEST(BinaryProtocolTest, MakeCommandEnvelope)
{
    MockCommand cmd{ 123, "hello" };
    auto envelope = make_command_envelope(42, cmd);

    EXPECT_EQ(envelope.id, 42u);
    EXPECT_EQ(envelope.message_type, "mock_command");
    EXPECT_FALSE(envelope.payload.empty());

    // Verify payload deserializes correctly.
    auto deserialized_cmd = deserialize_payload<MockCommand>(envelope.payload);
    EXPECT_EQ(deserialized_cmd.param1, 123);
    EXPECT_EQ(deserialized_cmd.param2, "hello");
}

TEST(BinaryProtocolTest, MakeResponseEnvelopeSuccess)
{
    Result<TestOkay, ApiError> result = Result<TestOkay, ApiError>::okay(TestOkay{ 99, "success" });
    auto envelope = make_response_envelope(42, "test_command", result);

    EXPECT_EQ(envelope.id, 42u);
    EXPECT_EQ(envelope.message_type, "test_command_response");
    EXPECT_FALSE(envelope.payload.empty());
}

TEST(BinaryProtocolTest, MakeResponseEnvelopeError)
{
    Result<TestOkay, ApiError> result = Result<TestOkay, ApiError>::error(ApiError{ "Failed!" });
    auto envelope = make_response_envelope(42, "test_command", result);

    EXPECT_EQ(envelope.id, 42u);
    EXPECT_EQ(envelope.message_type, "test_command_response");
    EXPECT_FALSE(envelope.payload.empty());
}

TEST(BinaryProtocolTest, ExtractResultSuccess)
{
    // Create a response envelope with a success result.
    Result<TestOkay, ApiError> original =
        Result<TestOkay, ApiError>::okay(TestOkay{ 777, "extracted" });
    auto envelope = make_response_envelope(1, "test", original);

    // Extract the result.
    auto extracted = extract_result<TestOkay, ApiError>(envelope);

    EXPECT_TRUE(extracted.isValue());
    EXPECT_EQ(extracted.value().value, 777);
    EXPECT_EQ(extracted.value().name, "extracted");
}

TEST(BinaryProtocolTest, ExtractResultError)
{
    // Create a response envelope with an error result.
    Result<TestOkay, ApiError> original =
        Result<TestOkay, ApiError>::error(ApiError{ "Extraction failed" });
    auto envelope = make_response_envelope(1, "test", original);

    // Extract the result.
    auto extracted = extract_result<TestOkay, ApiError>(envelope);

    EXPECT_TRUE(extracted.isError());
    EXPECT_EQ(extracted.errorValue().message, "Extraction failed");
}

// ============================================================================
// Full Roundtrip Test
// ============================================================================

TEST(BinaryProtocolTest, FullCommandResponseRoundtrip)
{
    // Simulate a full command/response cycle.

    // 1. Client creates command envelope.
    MockCommand cmd{ 42, "request" };
    auto cmd_envelope = make_command_envelope(123, cmd);

    // 2. Serialize for wire.
    auto wire_bytes = serialize_envelope(cmd_envelope);

    // 3. Server deserializes envelope.
    auto received_envelope = deserialize_envelope(wire_bytes);
    EXPECT_EQ(received_envelope.id, 123u);
    EXPECT_EQ(received_envelope.message_type, "mock_command");

    // 4. Server deserializes command payload.
    auto received_cmd = deserialize_payload<MockCommand>(received_envelope.payload);
    EXPECT_EQ(received_cmd.param1, 42);
    EXPECT_EQ(received_cmd.param2, "request");

    // 5. Server creates response.
    Result<TestOkay, ApiError> response =
        Result<TestOkay, ApiError>::okay(TestOkay{ 84, "response" });
    auto resp_envelope = make_response_envelope(received_envelope.id, "mock_command", response);

    // 6. Serialize response for wire.
    auto resp_wire_bytes = serialize_envelope(resp_envelope);

    // 7. Client deserializes response envelope.
    auto received_resp = deserialize_envelope(resp_wire_bytes);
    EXPECT_EQ(received_resp.id, 123u);
    EXPECT_EQ(received_resp.message_type, "mock_command_response");

    // 8. Client extracts result.
    auto result = extract_result<TestOkay, ApiError>(received_resp);
    EXPECT_TRUE(result.isValue());
    EXPECT_EQ(result.value().value, 84);
    EXPECT_EQ(result.value().name, "response");
}

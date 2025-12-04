# WebSocket Client Library Design

## Current State

We currently have **two separate** WebSocketClient implementations:

1. **UI WebSocketClient** (`src/ui/state-machine/network/WebSocketClient.{h,cpp}`)
   - Wraps `rtc::WebSocket` from libdatachannel
   - Sends commands to DSSM server (port 8080)
   - Receives binary RenderMessages, converts to UiUpdateEvents
   - Integrated with UI EventSink for state machine routing
   - Frame throttling (60 FPS max)
   - Blocking `sendAndReceive(string) -> string` for request/response

2. **CLI WebSocketClient** (`src/cli/WebSocketClient.{h,cpp}`)
   - Also wraps `rtc::WebSocket`
   - Sends commands to server or UI
   - Simple blocking send/receive
   - No EventSink dependency
   - Used for scripting and testing

**Code duplication:** Both implement similar core functionality (connect, send, receive, timeout handling) but can't share code.

## Problems

1. **Duplication**: Connection management, send/receive logic duplicated between UI and CLI clients
2. **Type safety**: Both use `bool` or `string` returns, losing error context
3. **Inflexibility**: Can't easily add new clients (test harnesses, peer-to-peer, monitoring tools)
4. **Coupling**: UI client tightly coupled to EventSink and UiUpdateEvent
5. **Error handling**: Empty string `""` or `false` loses information about what failed

## Proposed Architecture

Create a **general-purpose WebSocketClient library** that any component can use:

```
┌─────────────────────────────────────────────────────────────────┐
│  WebSocketClient (general library)                              │
│                                                                  │
│  Core Features:                                                 │
│  - Connection management (connect, disconnect, isConnected)     │
│  - Send text/binary messages                                    │
│  - Async receive via callbacks                                  │
│  - Type-safe command/response (template-based)                  │
│  - Proper error handling (Result types)                         │
│  - No component-specific dependencies                           │
└─────────────────────────────────────────────────────────────────┘
                              ↑
                              │ (use as-is or wrap)
        ┌─────────────────────┼─────────────────────┐
        │                     │                     │
┌───────────────┐   ┌─────────────────┐   ┌────────────────┐
│  UiClient     │   │  CliClient      │   │  PeerClient    │
│  (+ EventSink)│   │  (simple)       │   │  (p2p comms)   │
└───────────────┘   └─────────────────┘   └────────────────┘
```

**Specific wrappers** add component-specific features:
- UiClient: EventSink integration, RenderMessage → UiUpdateEvent conversion
- CliClient: Simple blocking semantics for scripts
- PeerClient: Server-to-server communication

## API Design

### Core WebSocketClient

```cpp
namespace DirtSim {
namespace Network {

class WebSocketClient {
public:
    // Connection management.
    Result<void, std::string> connect(const std::string& url);
    void disconnect();
    bool isConnected() const;
    std::string getUrl() const;

    // Raw send (returns immediately).
    Result<void, std::string> send(const std::string& message);
    Result<void, std::string> sendBinary(const std::vector<std::byte>& data);

    // Blocking send and receive with timeout.
    Result<std::string, std::string> sendAndReceive(
        const std::string& message,
        int timeoutMs = 5000);

    // Type-safe command/response (blocking).
    template <ApiCommandType CommandT>
    Result<typename CommandT::OkayType, std::string> sendAndReceive(
        const CommandT& cmd,
        int timeoutMs = 5000)
    {
        // Serialize command to JSON.
        nlohmann::json json = cmd.toJson();
        json["command"] = CommandT::name();

        // Send and wait for response.
        auto jsonResult = sendAndReceive(json.dump(), timeoutMs);
        if (jsonResult.isError()) {
            return Result<typename CommandT::OkayType, std::string>::error(
                jsonResult.errorValue());
        }

        // Parse response JSON.
        nlohmann::json responseJson;
        try {
            responseJson = nlohmann::json::parse(jsonResult.value());
        } catch (const std::exception& e) {
            return Result<typename CommandT::OkayType, std::string>::error(
                std::string("Invalid JSON response: ") + e.what());
        }

        // Check for error response.
        if (responseJson.contains("error")) {
            return Result<typename CommandT::OkayType, std::string>::error(
                responseJson["error"].get<std::string>());
        }

        // Deserialize success response.
        if (!responseJson.contains("value")) {
            return Result<typename CommandT::OkayType, std::string>::error(
                "Response missing 'value' field");
        }

        try {
            // Handle monostate vs structured response.
            if constexpr (std::is_same_v<typename CommandT::OkayType, std::monostate>) {
                return Result<typename CommandT::OkayType, std::string>::okay(
                    std::monostate{});
            } else {
                auto okay = ReflectSerializer::from_json<typename CommandT::OkayType>(
                    responseJson["value"]);
                return Result<typename CommandT::OkayType, std::string>::okay(
                    std::move(okay));
            }
        } catch (const std::exception& e) {
            return Result<typename CommandT::OkayType, std::string>::error(
                std::string("Failed to deserialize response: ") + e.what());
        }
    }

    // Async receive via callback (for streaming/events).
    using MessageCallback = std::function<void(const std::string&)>;
    using BinaryCallback = std::function<void(const std::vector<std::byte>&)>;
    void onMessage(MessageCallback callback);
    void onBinary(BinaryCallback callback);

    // Connection event callbacks.
    using ConnectionCallback = std::function<void()>;
    using ErrorCallback = std::function<void(const std::string&)>;
    void onConnected(ConnectionCallback callback);
    void onDisconnected(ConnectionCallback callback);
    void onError(ErrorCallback callback);

private:
    std::shared_ptr<rtc::WebSocket> ws_;
    std::string url_;

    // For blocking sendAndReceive.
    std::string response_;
    bool responseReceived_;
    std::mutex responseMutex_;
    std::condition_variable responseCv_;

    // Callbacks.
    MessageCallback messageCallback_;
    BinaryCallback binaryCallback_;
    ConnectionCallback connectedCallback_;
    ConnectionCallback disconnectedCallback_;
    ErrorCallback errorCallback_;
};

} // namespace Network
} // namespace DirtSim
```

### Component-Specific Wrappers

**UI Client (with EventSink integration):**
```cpp
namespace DirtSim {
namespace Ui {

class UiWebSocketClient : public Network::WebSocketClient {
public:
    void setEventSink(EventSink* sink);

private:
    EventSink* eventSink_;

    // Override binary callback to convert RenderMessage -> UiUpdateEvent.
    void handleBinaryMessage(const std::vector<std::byte>& data);
};

} // namespace Ui
} // namespace DirtSim
```

**CLI Client (simple pass-through):**
```cpp
// CLI can just use Network::WebSocketClient directly, no wrapper needed.
```

## Migration Plan

### Phase 1: Create General Library
1. Create `src/core/network/WebSocketClient.{h,cpp}` with general implementation
2. Extract common code from UI and CLI clients
3. Implement type-safe `sendAndReceive<CommandT>`
4. Write unit tests

### Phase 2: Migrate CLI Client
1. Replace `src/cli/WebSocketClient` with `Network::WebSocketClient`
2. Update all CLI call sites
3. Test CLI tools (benchmark, integration tests, manual commands)

### Phase 3: Migrate UI Client
1. Create `UiWebSocketClient` wrapper that adds EventSink integration
2. Replace `src/ui/state-machine/network/WebSocketClient` with wrapper
3. Update all UI call sites
4. Test UI interactions

### Phase 4: Cleanup
1. Remove old client implementations
2. Update documentation
3. Consider adding similar general WebSocketServer library

## Benefits

1. **Code reuse**: Write connection/send/receive logic once
2. **Type safety**: `Result<OkayType, Error>` instead of `bool`/`string`
3. **Better errors**: Know exactly what failed (timeout, disconnected, parse error, etc.)
4. **Flexibility**: Easy to add new clients (test harnesses, peer discovery, monitoring)
5. **Consistency**: Same API across all components
6. **Maintainability**: Bug fixes and improvements benefit all clients

## Open Questions

1. Should the general client be header-only (template-heavy) or compiled?
2. How to handle binary message routing (some clients want it, some don't)?
3. Should we also generalize WebSocketServer?
4. Namespace: `DirtSim::Network` or `DirtSim::Core`?
5. Do we need async (non-blocking) send with futures/callbacks?

## Implementation Notes

- Keep the existing EventSink integration working (don't break UI)
- Use `rtc::WebSocket` as the underlying transport (already working well)
- Type-safe template methods should be header-only for zero-cost abstraction
- Blocking methods can be in .cpp file
- Consider thread safety for blocking receive (mutex + condition variable)

## Related Work

- Current WebSocket clients already use `rtc::WebSocket` from libdatachannel
- ResponseSerializerJson and CommandDeserializerJson provide (de)serialization
- All API commands now have `OkayType` aliases (preparation for this work)

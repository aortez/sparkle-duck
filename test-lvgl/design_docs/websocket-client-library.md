# WebSocket Client Library Design

## ✅ IMPLEMENTATION STATUS (2025-12-05)

**COMPLETE:** WebSocketService unified architecture - MAJOR REFACTOR!

### 🌱 What's Working (2025-12-05)

**WebSocketService (Network::WebSocketService)**
- ✅ Renamed from WebSocketClient to WebSocketService
- ✅ Unified client + server roles in single class
- ✅ Binary-only protocol (JSON removed from internal communication)
- ✅ `listen(port)` - Server-side listening
- ✅ `registerHandler<CwcT>(handler)` - Type-safe command handler registration
- ✅ `broadcastBinary(data)` - Broadcast to all connected clients
- ✅ CommandWithCallback integration for async handlers

**Server Migration (Server::StateMachine)**
- ✅ All 20 API commands registered in `setupWebSocketService()`
- ✅ Immediate handlers: StateGet, StatusGet, RenderFormatGet, RenderFormatSet
- ✅ Queued handlers: All others queue to state machine
- ✅ RenderMessage broadcasting working
- ✅ Old Server::WebSocketServer unused (ready for deletion)

**CLI Refactor (Client::CommandDispatcher)**
- ✅ Type-safe dispatcher using template metaprogramming
- ✅ Auto-registers commands with response deserializers
- ✅ Fluent syntax: `cli server StatusGet`, `cli ui SimPause`
- ✅ Default addresses (no more typing URLs!)
- ✅ Binary protocol with full response data display
- ✅ CamelCase command names everywhere

**UI Migration (In Progress)**
- ✅ UI sends binary commands to server (SimRun, etc.)
- ✅ UI receives RenderMessages via WebSocketService
- ✅ Single connection (replaces dual client/server)
- ✅ Rendering works, simulation runs
- ⏳ Old Ui::WebSocketClient still present (ready for removal)

### Known Issues
1. **libdatachannel buffering:** RenderMessages arrive in bursts with ~2 second initial delay. Messages buffer in libdatachannel WebSocket and deliver all at once instead of streaming. System works but rendering startup is delayed. Need to investigate rtc::WebSocketConfiguration settings.
2. **Response serialization:** Some commands (PeersGet, TimerStatsGet) have complex response types that ReflectSerializer can't auto-serialize. Need custom toJson() implementations.
3. **Old code cleanup:** Ui::WebSocketClient, Ui::WebSocketServer, and Server::WebSocketServer still in codebase but unused. Ready for deletion.

---

## Original Problem Statement

We previously had **two separate** WebSocketClient implementations:

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

See **Migration Phases** in the "Binary Protocol: All-in on zpp_bits" section below for the detailed plan.

**Summary:**
1. **Phase 1a:** Define `MessageEnvelope` struct and test serialization.
2. **Phase 1b:** Add dual-format support to server (JSON + binary).
3. **Phase 2:** Create general `WebSocketClient` with binary support.
4. **Phase 3:** Migrate CLI to binary protocol.
5. **Phase 4:** Migrate UI client.
6. **Phase 5:** Optional cleanup of JSON path.

## Benefits

1. **Code reuse**: Write connection/send/receive logic once
2. **Type safety**: `Result<OkayType, Error>` instead of `bool`/`string`
3. **Better errors**: Know exactly what failed (timeout, disconnected, parse error, etc.)
4. **Flexibility**: Easy to add new clients (test harnesses, peer discovery, monitoring)
5. **Consistency**: Same API across all components
6. **Maintainability**: Bug fixes and improvements benefit all clients

## Binary Protocol: All-in on zpp_bits

### Motivation

We've evolved through several serialization formats:
1. **JSON** - Universal, human-readable, but verbose and slow for large data.
2. **MessagePack** - More compact, but still too slow for frame data.
3. **zpp_bits** - Blazing fast, zero-copy, compile-time reflection. Currently used for RenderMessages.

**Decision:** Go all-in on zpp_bits for command/response as well as world data.

**Rationale:**
- **Performance**: zpp_bits is significantly faster than JSON/MessagePack.
- **Consistency**: One serialization format everywhere simplifies code and mental model.
- **CLI as universal interface**: External tools can use CLI to send commands; they don't need to speak binary directly.
- **Existing infrastructure**: Command structs are aggregates that zpp_bits handles automatically.

### Binary Protocol Design

#### Envelope Struct

A single unified envelope for both commands and responses:

```cpp
// src/core/network/BinaryProtocol.h

namespace DirtSim {
namespace Network {

/// Unified message envelope for binary protocol.
/// Works for both commands (client→server) and responses (server→client).
struct MessageEnvelope {
    uint64_t id;                      // Correlation ID for request/response matching.
    std::string message_type;         // Message type ("state_get", "state_get_response", etc.).
    std::vector<std::byte> payload;   // zpp_bits serialized content.
};

} // namespace Network
} // namespace DirtSim
```

**For commands:**
- `message_type` = command name (e.g., "state_get", "sim_run")
- `payload` = zpp_bits serialized `Command` struct

**For responses:**
- `message_type` = command name + "_response" (e.g., "state_get_response")
- `payload` = zpp_bits serialized `Result<OkayType, ApiError>`

The `Result` type already encodes success/failure, so no separate `is_error` flag needed.

#### Wire Format

WebSocket frame types determine the protocol:
- **Text frame** → JSON protocol (legacy, for backward compatibility).
- **Binary frame** → zpp_bits protocol (new, preferred).

This enables a smooth dual-format transition period.

#### Message Flow

```
Client                                      Server
──────                                      ──────
Command struct
    │
    ▼
serialize Command to payload (zpp_bits)
    │
    ▼
MessageEnvelope{id, "state_get", payload}
    │
    ▼
serialize envelope (zpp_bits)
    │
    ▼
send as binary frame ─────────────────────► receive binary frame
                                                │
                                                ▼
                                           deserialize envelope (zpp_bits)
                                                │
                                                ▼
                                           route by message_type
                                                │
                                                ▼
                                           deserialize payload to Command type
                                                │
                                                ▼
                                           execute handler → Result<OkayType, ApiError>
                                                │
                                                ▼
                                           serialize Result to payload (zpp_bits)
                                                │
                                                ▼
                                           MessageEnvelope{id, "state_get_response", payload}
                                                │
                                                ▼
receive binary frame ◄───────────────────── send as binary frame
    │
    ▼
deserialize envelope (zpp_bits)
    │
    ▼
deserialize payload to Result<OkayType, ApiError>
    │
    ▼
return Result to caller
```

### Dual-Format Migration Strategy

The key insight: **nothing breaks during migration**. Server learns to speak both protocols, then clients migrate one at a time.

```
┌─────────────────────────────────────┐
│            Server                    │
│  ┌─────────────────────────────┐    │
│  │     Message Handler         │    │
│  │  ┌───────────┬───────────┐  │    │
│  │  │ Text Frame│Binary Frame│ │    │
│  │  │  (JSON)   │ (zpp_bits) │ │    │
│  │  └─────┬─────┴─────┬─────┘  │    │
│  │        │           │        │    │
│  │        ▼           ▼        │    │
│  │   JSON Handler  Binary      │    │
│  │   (existing)    Handler     │    │
│  │                 (new)       │    │
│  └─────────────────────────────┘    │
│                                      │
│  Response in SAME FORMAT as request  │
└─────────────────────────────────────┘
          ▲                 ▲
          │                 │
     JSON │                 │ Binary
          │                 │
   ┌──────┴──┐         ┌────┴────┐
   │Old Client│         │New Client│
   │  (JSON)  │         │(zpp_bits)│
   └──────────┘         └──────────┘
```

### Migration Phases

#### Phase 1a: Define Binary Protocol (no behavior change)
- Create `src/core/network/BinaryProtocol.h` with unified `MessageEnvelope` struct.
- Add zpp_bits serialization support.
- Unit tests for envelope serialization roundtrip.
- Unit tests for `Result<T, ApiError>` serialization roundtrip.

#### Phase 1b: Server Dual-Format Support (backward compatible)
- Modify server message handler to detect frame type.
- Text frame → existing JSON path (unchanged).
- Binary frame → new zpp_bits path.
- Response uses same format as request.
- All existing tests pass (they use JSON).
- Add new tests for binary path.

#### Phase 2: General WebSocketClient Library
- Create `src/core/network/WebSocketClient.{h,cpp}`.
- Support both JSON and binary protocols.
- Add `sendCommand<T>` with format selection (default: binary).
- Result<> return types for proper error handling.
- Correlation ID support for multiplexed requests.

#### Phase 3: Migrate CLI Client
- Replace `src/cli/WebSocketClient` with `Network::WebSocketClient`.
- CLI uses binary protocol by default.
- Add `--json` flag for debugging (sends JSON instead).
- Test all CLI commands work with binary protocol.

#### Phase 4: Migrate UI Client
- Create thin `UiWebSocketClient` wrapper.
- Adds EventSink integration for RenderMessage routing.
- Uses binary protocol for commands.
- Test UI interactions.

#### Phase 5: Cleanup (optional)
- Consider removing JSON command path from server.
- Or keep it for debugging/external tool compatibility.
- Update documentation.

### CLI as Universal Interface

External tools that need to communicate with the system can use CLI:

```bash
# CLI handles binary protocol internally, presents JSON to user.
./cli state_get ws://localhost:8080
# Output: {"worldData": {...}}

# Or pipe JSON in.
echo '{"scenario_id": "sandbox"}' | ./cli sim_run ws://localhost:8080
```

This keeps the binary protocol as an internal optimization while maintaining a human-friendly interface.

## Resolved Questions

1. **Header-only vs compiled?** Template methods (sendCommand<T>) in header, blocking logic in .cpp.
2. **Binary message routing?** Callbacks let each client decide how to handle binary data.
3. **Namespace?** `DirtSim::Network` - it's specifically about networking.
4. **Serialization format?** zpp_bits for everything (commands and world data).

## Open Questions

1. Should we also generalize WebSocketServer with the same dual-format support?
2. Do we need async (non-blocking) send with futures/callbacks?
3. Should CLI keep a `--json` debug mode, or is that unnecessary complexity?

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

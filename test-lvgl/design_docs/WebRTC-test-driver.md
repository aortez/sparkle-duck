# WebRTC Test Driver Implementation Plan

## Overview

This document outlines the implementation plan for a WebRTC-based test driver for the Sparkle Duck physics simulation. The test driver will provide real-time bidirectional communication between test clients and the simulation, enabling automated testing, remote control, and live monitoring capabilities.

## Goals

### Primary Use Cases
1. **Automated Test Driver**: Execute test sequences, capture world state, validate simulation behavior
2. **Remote Control Panel**: Real-time control of simulation parameters from web browsers
3. **Live Monitoring**: Stream simulation state and visual data to external applications
4. **Debugging Interface**: Step-by-step simulation control and state inspection

### Key Features
- Advance simulation by N frames
- Capture complete grid state dumps (JSON)
- Control simulation parameters (gravity, pressure, materials)
- Stream visual snapshots/screenshots
- Real-time bidirectional communication
- Multiple concurrent client connections

## Architecture

### WebRTC Technology Choice: libdatachannel

**Selected Library**: [libdatachannel](https://github.com/paullouisageneau/libdatachannel)
- **Rationale**: Lightweight, CMake-friendly, data channels only (no A/V overhead)
- **License**: MPL 2.0 (compatible with project)
- **Dependencies**: Minimal (OpenSSL, libjuice for ICE)
- **Platform**: Linux-first, cross-platform capable

### Core Components

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   Test Client   │◄──►│  WebRTC Worker   │◄──►│ Command Queue   │
│   (Browser/CLI) │    │     Thread       │    │ (Thread-Safe)   │
└─────────────────┘    └──────────────────┘    └─────────────────┘
                                                         │
                       ┌──────────────────┐             ▼
                       │ State Machine    │    ┌─────────────────┐
                       │ (Sim States)     │◄──►│ World Interface │
                       └──────────────────┘    │ (Main Thread)   │
                                ▲               └─────────────────┘
                                │                        │
                       ┌──────────────────┐             ▼
                       │ Simulation Loop  │    ┌─────────────────┐
                       │   Integration    │◄──►│ Physics Engine  │
                       └──────────────────┘    └─────────────────┘
```

### Threading Model and Concurrency

#### WebRTC Worker Thread
- **Purpose**: Handles WebRTC events, message parsing, and network I/O without blocking simulation
- **Responsibilities**: DataChannel events, signaling, client connection management
- **Communication**: Thread-safe command queue to main simulation thread
- **Isolation**: No direct world state access - all commands go through queue

#### Main Simulation Thread
- **Purpose**: Runs physics simulation, processes queued commands, updates UI
- **Command Processing**: Dequeues and executes commands between simulation frames
- **State Synchronization**: Commands execute atomically during stable simulation states
- **Response Handling**: Sends results back through WebRTC manager

### Command Queue Design

#### Queue Implementation
```cpp
// Thread-safe command queue between WebRTC and simulation threads
class CommandQueue {
    struct Command {
        std::string clientId;
        std::string requestId;
        CommandType type;
        rapidjson::Document params;
        std::promise<rapidjson::Document> response;
    };
    
    std::queue<Command> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> shutdown_{false};
    
public:
    void enqueue(Command&& cmd);
    std::optional<Command> dequeue(std::chrono::milliseconds timeout);
    void shutdown();
};
```

#### Command Execution Flow
1. **WebRTC Thread**: Receives message, validates, enqueues command with future/promise
2. **Main Thread**: Dequeues during stable simulation state, executes command
3. **Response**: Results sent back through promise, WebRTC thread sends to client
4. **Error Handling**: Command validation failures returned immediately, execution errors queued

### Simulation State Machine

#### Core States
- **IDLE**: Ready for any command, simulation paused
- **RUNNING**: Simulation active, limited command set available
- **STEPPING**: Manual step mode, waiting for step commands
- **RESETTING**: World reset in progress, commands queued until complete
- **ERROR**: Simulation error state, only reset/diagnostics available

#### State-Dependent Command Availability
```cpp
enum class SimulationState { IDLE, RUNNING, STEPPING, RESETTING, ERROR };

// Commands available in each state
constexpr std::array<CommandType, 3> IDLE_COMMANDS = {STEP, RUN, RESET};
constexpr std::array<CommandType, 2> RUNNING_COMMANDS = {PAUSE, GET_STATE};
constexpr std::array<CommandType, 4> STEPPING_COMMANDS = {STEP, RUN, RESET, GET_STATE};
```

#### State Transitions
- **IDLE → RUNNING**: Via `run` command, simulation starts automatically
- **RUNNING → IDLE**: Via `pause` command or user interaction
- **IDLE → STEPPING**: Via `step` command with manual mode flag
- **ANY → RESETTING**: Via `reset` command, returns to IDLE when complete
- **ANY → ERROR**: On simulation failures, requires reset to recover

### Variant-Based State Machine Implementation

#### State Data Structures
Each state maintains its own relevant data using typed structs:

```cpp
#include <variant>
#include <chrono>
#include <vector>
#include <string>

// State-specific data containers
struct IdleStateData {
    std::chrono::steady_clock::time_point idle_since;
    bool ui_controls_enabled = true;
    uint32_t last_timestep = 0;  // Snapshot for resuming
};

struct RunningStateData {
    std::chrono::steady_clock::time_point started_at;
    uint32_t frames_processed = 0;
    double target_fps = 60.0;
    bool auto_stepping = true;
    uint32_t total_steps_since_start = 0;
};

struct SteppingStateData {
    uint32_t pending_steps = 0;
    uint32_t steps_completed = 0;
    std::string requesting_client_id;
    bool manual_mode = true;
    std::chrono::steady_clock::time_point step_request_time;
};

struct ResettingStateData {
    std::chrono::steady_clock::time_point reset_started;
    enum class ResetPhase { 
        CLEARING_WORLD, 
        REINIT_PHYSICS, 
        RESTORING_UI,
        NOTIFYING_CLIENTS 
    } phase = ResetPhase::CLEARING_WORLD;
    std::vector<std::string> waiting_clients;  // Clients to notify on completion
    uint32_t reset_progress_percent = 0;
};

struct ErrorStateData {
    std::string error_message;
    std::chrono::steady_clock::time_point error_time;
    std::exception_ptr exception_ptr;
    SimulationStateType previous_state_type;  // For potential recovery
    bool recovery_attempted = false;
};
```

#### State Machine Class
```cpp
class SimulationStateMachine {
public:
    using StateVariant = std::variant<
        IdleStateData, 
        RunningStateData, 
        SteppingStateData, 
        ResettingStateData, 
        ErrorStateData
    >;

private:
    StateVariant current_state_;
    std::chrono::steady_clock::time_point last_transition_;

public:
    SimulationStateMachine() 
        : current_state_(IdleStateData{.idle_since = std::chrono::steady_clock::now()})
        , last_transition_(std::chrono::steady_clock::now()) {}

    // Type-safe state transitions with data initialization
    void transitionToRunning(double target_fps = 60.0) {
        last_transition_ = std::chrono::steady_clock::now();
        current_state_ = RunningStateData{
            .started_at = std::chrono::steady_clock::now(),
            .target_fps = target_fps,
            .auto_stepping = true
        };
    }

    void transitionToStepping(const std::string& client_id, uint32_t step_count) {
        last_transition_ = std::chrono::steady_clock::now();
        current_state_ = SteppingStateData{
            .pending_steps = step_count,
            .requesting_client_id = client_id,
            .step_request_time = std::chrono::steady_clock::now()
        };
    }

    void transitionToError(const std::string& error_msg, SimulationStateType previous) {
        last_transition_ = std::chrono::steady_clock::now();
        current_state_ = ErrorStateData{
            .error_message = error_msg,
            .error_time = std::chrono::steady_clock::now(),
            .previous_state_type = previous
        };
    }

    // Visitor pattern for state-specific operations
    template<typename Visitor>
    auto visit(Visitor&& visitor) -> decltype(visitor(std::get<IdleStateData>(current_state_))) {
        return std::visit(std::forward<Visitor>(visitor), current_state_);
    }

    // Type-safe state data access
    template<typename StateType>
    StateType* getCurrentStateData() {
        return std::get_if<StateType>(&current_state_);
    }

    template<typename StateType>
    const StateType* getCurrentStateData() const {
        return std::get_if<StateType>(&current_state_);
    }

    // Get current state type for command validation
    SimulationStateType getCurrentStateType() const {
        return visit([](const auto& state) -> SimulationStateType {
            using T = std::decay_t<decltype(state)>;
            if constexpr (std::is_same_v<T, IdleStateData>) return SimulationStateType::IDLE;
            else if constexpr (std::is_same_v<T, RunningStateData>) return SimulationStateType::RUNNING;
            else if constexpr (std::is_same_v<T, SteppingStateData>) return SimulationStateType::STEPPING;
            else if constexpr (std::is_same_v<T, ResettingStateData>) return SimulationStateType::RESETTING;
            else if constexpr (std::is_same_v<T, ErrorStateData>) return SimulationStateType::ERROR;
        });
    }
};
```

#### Command Validation with State Data
```cpp
class WebRTCCommandHandler {
private:
    SimulationStateMachine* state_machine_;

    bool isCommandValidInCurrentState(CommandType cmd) const {
        return state_machine_->visit([cmd](const auto& state_data) -> bool {
            using StateType = std::decay_t<decltype(state_data)>;
            
            if constexpr (std::is_same_v<StateType, IdleStateData>) {
                return cmd == CommandType::STEP || cmd == CommandType::RUN || 
                       cmd == CommandType::RESET || cmd == CommandType::GET_STATE;
            }
            else if constexpr (std::is_same_v<StateType, RunningStateData>) {
                return cmd == CommandType::PAUSE || cmd == CommandType::GET_STATE || 
                       cmd == CommandType::SCREENSHOT;
            }
            else if constexpr (std::is_same_v<StateType, SteppingStateData>) {
                return cmd == CommandType::STEP || cmd == CommandType::RUN || 
                       cmd == CommandType::RESET || cmd == CommandType::GET_STATE;
            }
            else if constexpr (std::is_same_v<StateType, ResettingStateData>) {
                return cmd == CommandType::GET_STATE;  // Only status queries during reset
            }
            else if constexpr (std::is_same_v<StateType, ErrorStateData>) {
                return cmd == CommandType::RESET || cmd == CommandType::GET_STATE;
            }
            return false;
        });
    }

    void executeStepCommand(const Command& cmd) {
        if (auto* stepping_data = state_machine_->getCurrentStateData<SteppingStateData>()) {
            stepping_data->pending_steps += cmd.params["count"].GetUint();
            // Send response with updated step count
        } else {
            // Transition to stepping mode
            uint32_t step_count = cmd.params["count"].GetUint();
            state_machine_->transitionToStepping(cmd.clientId, step_count);
        }
    }
};
```

#### State-Specific Behavior Examples
```cpp
// In simulation loop integration
void processFrame(WorldInterface& world, LoopState& state) {
    state.state_machine->visit([&](auto& state_data) {
        using StateType = std::decay_t<decltype(state_data)>;
        
        if constexpr (std::is_same_v<StateType, RunningStateData>) {
            // Auto-advance simulation
            world.advanceOneTimestep();
            state_data.frames_processed++;
            state_data.total_steps_since_start++;
            
            // Check if we need to throttle based on target_fps
            auto frame_duration = std::chrono::steady_clock::now() - state_data.started_at;
            auto expected_frames = frame_duration.count() * state_data.target_fps / 1000000000.0;
            
            if (state_data.frames_processed > expected_frames + 5) {
                // Running too fast, yield CPU
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        else if constexpr (std::is_same_v<StateType, SteppingStateData>) {
            // Only advance if steps are pending
            if (state_data.pending_steps > 0) {
                world.advanceOneTimestep();
                state_data.pending_steps--;
                state_data.steps_completed++;
                
                // Check if stepping complete
                if (state_data.pending_steps == 0) {
                    // Send completion response to requesting client
                    sendStepCompletionResponse(state_data.requesting_client_id, 
                                             state_data.steps_completed);
                }
            }
        }
        else if constexpr (std::is_same_v<StateType, ResettingStateData>) {
            // Handle reset phases
            switch (state_data.phase) {
                case ResettingStateData::ResetPhase::CLEARING_WORLD:
                    world.reset();
                    state_data.phase = ResettingStateData::ResetPhase::REINIT_PHYSICS;
                    state_data.reset_progress_percent = 25;
                    break;
                case ResettingStateData::ResetPhase::REINIT_PHYSICS:
                    // Reinitialize physics parameters
                    state_data.phase = ResettingStateData::ResetPhase::RESTORING_UI;
                    state_data.reset_progress_percent = 75;
                    break;
                // ... handle other phases
            }
        }
        // IdleStateData and ErrorStateData don't advance simulation
    });
}
```

#### Benefits of This Approach
1. **Type Safety**: Compile-time guarantees about accessing correct state data
2. **Memory Efficiency**: Only current state data is stored in memory
3. **Performance**: No virtual function calls or dynamic allocation
4. **Extensible**: Easy to add new states with their own data
5. **Debuggable**: State data is easily inspectable and serializable
6. **Thread-Safe**: State transitions can be made atomic with proper locking

## Implementation Plan

### Phase 1: Core WebRTC Infrastructure (Week 1)

#### 1.1 Dependency Setup
```cmake
# Add to CMakeLists.txt
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBDATACHANNEL REQUIRED libdatachannel)

# Or use FetchContent for source build
include(FetchContent)
FetchContent_Declare(
    libdatachannel
    GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
    GIT_TAG v0.19.0
)
FetchContent_MakeAvailable(libdatachannel)
```

#### 1.2 WebRTC Manager Class
```cpp
// src/WebRTCManager.h
class WebRTCManager {
public:
    WebRTCManager(CommandQueue* command_queue);
    ~WebRTCManager();
    
    bool initialize(uint16_t port = 8080);
    void shutdown();
    
    // Main thread calls this to process responses and send to clients
    void processOutgoingMessages();
    
    // Client management (called from WebRTC worker thread)
    void onClientConnected(const std::string& clientId);
    void onClientDisconnected(const std::string& clientId);
    void onMessage(const std::string& clientId, const std::string& message);
    
    // Broadcasting (called from main thread via command responses)
    void broadcastToAll(const std::string& message);
    void sendToClient(const std::string& clientId, const std::string& message);
    
private:
    // Worker thread for WebRTC event handling
    void workerThreadMain();
    std::thread worker_thread_;
    std::atomic<bool> shutdown_{false};
    
    // Command queue for thread communication
    CommandQueue* command_queue_;
    
    // WebRTC components (accessed from worker thread)
    std::unordered_map<std::string, rtc::PeerConnection> clients_;
    std::unique_ptr<rtc::WebSocket> signaling_server_;
    
    // Thread-safe message queues for responses
    struct OutgoingMessage {
        std::string clientId;
        std::string message;
    };
    std::queue<OutgoingMessage> outgoing_queue_;
    std::mutex outgoing_mutex_;
};
```

#### 1.3 WebSocket Signaling Server
- WebSocket-based signaling for WebRTC offer/answer/ICE exchange
- Uses existing RapidJSON (available via LVGL/ThorVG)
- Handles STUN/TURN configuration for NAT traversal
- Session management for multiple client connections

#### 1.4 WebRTC Implementation Details
- **Data Channels**: Reliable, ordered channels for command/response traffic
- **Connection Management**: Auto-reconnection logic for client disconnections
- **Message Framing**: JSON protocol with length prefixes for large state dumps
- **Flow Control**: Backpressure handling when client processing lags
- **Security**: Optional token-based authentication and CORS handling
- **ICE Configuration**: Public STUN servers for development, configurable TURN for production

### Phase 2: Command Protocol Design (Week 1)

#### 2.1 JSON Message Protocol

**Command Messages (Client → Simulation)**:
```json
{
  "type": "command",
  "id": "unique-request-id",
  "command": "step",
  "params": {
    "count": 100
  }
}
```

**Response Messages (Simulation → Client)**:
```json
{
  "type": "response", 
  "requestId": "unique-request-id",
  "success": true,
  "data": {
    "timestep": 1234,
    "totalMass": 1500.5
  }
}
```

**Event Messages (Simulation → Client)**:
```json
{
  "type": "event",
  "event": "stateChanged",
  "data": {
    "timestep": 1235,
    "fps": 60
  }
}
```

#### 2.2 Command Set

| Command | Description | Parameters | Response |
|---------|-------------|------------|----------|
| `step` | Advance N simulation frames | `count: number` | `{timestep, totalMass}` |
| `reset` | Reset world to initial state | none | `{success: boolean}` |
| `pause` | Pause/resume simulation | `paused: boolean` | `{paused: boolean}` |
| `getState` | Get complete world state | `format: "grid"/"summary"` | Grid data or summary |
| `setParam` | Set simulation parameter | `param: string, value: number` | `{success, oldValue}` |
| `screenshot` | Capture visual snapshot | `format: "png"/"raw"` | Base64 image data |
| `addMaterial` | Add material at position | `x, y, material, amount` | `{success: boolean}` |

### Phase 3: World Interface Integration (Week 2)

#### 3.1 Command Handler Integration
```cpp
// src/WebRTCCommandHandler.h
class WebRTCCommandHandler {
public:
    WebRTCCommandHandler(WorldInterface* world, WebRTCManager* manager);
    
    void handleCommand(const std::string& clientId, const rapidjson::Document& command);
    
private:
    void handleStep(const std::string& clientId, const rapidjson::Value& params);
    void handleGetState(const std::string& clientId, const rapidjson::Value& params);
    void handleScreenshot(const std::string& clientId, const rapidjson::Value& params);
    void handleSetParam(const std::string& clientId, const rapidjson::Value& params);
    
    WorldInterface* world_;
    WebRTCManager* manager_;
};
```

#### 3.2 State Serialization
- Implement grid state dumping using existing `getCellInterface()` methods
- Support both RulesA (World) and RulesB (WorldB) formats
- Efficient JSON serialization for large grids (chunking/compression)

#### 3.3 Screenshot Integration
- Leverage LVGL's `lv_snapshot` functionality
- PNG encoding using existing lodepng (available in LVGL)
- Base64 encoding for WebRTC data channel transmission

### Phase 4: Simulation Loop Integration (Week 2)

#### 4.1 Command Queue Integration
```cpp
// Integration with existing simulator_loop.h
namespace SimulatorLoop {
    struct LoopState {
        // ... existing fields ...
        CommandQueue* command_queue = nullptr;
        WebRTCManager* webrtc_manager = nullptr;
        SimulationState sim_state = SimulationState::IDLE;
        WebRTCCommandHandler* command_handler = nullptr;
    };
    
    // Modified processFrame function
    inline void processFrame(WorldInterface& world, LoopState& state, uint32_t delta_time_ms = 16) {
        // Process queued WebRTC commands during stable states
        if (state.command_queue && state.sim_state != SimulationState::RESETTING) {
            processQueuedCommands(world, state);
        }
        
        // Process outgoing WebRTC messages
        if (state.webrtc_manager) {
            state.webrtc_manager->processOutgoingMessages();
        }
        
        // Only advance simulation if in RUNNING state or processing STEP commands
        if (state.sim_state == SimulationState::RUNNING || 
            (state.sim_state == SimulationState::STEPPING && hasStepCommands(state))) {
            // ... existing simulation logic ...
            
            // Broadcast state changes to connected clients
            broadcastStateUpdate(state, world.getTimestep());
        }
    }
    
    void processQueuedCommands(WorldInterface& world, LoopState& state) {
        constexpr auto timeout = std::chrono::milliseconds(1);
        while (auto cmd = state.command_queue->dequeue(timeout)) {
            if (isCommandValidInState(cmd->type, state.sim_state)) {
                state.command_handler->executeCommand(world, *cmd, state);
            } else {
                // Send error response: command not valid in current state
                sendCommandError(*cmd, "Command not available in current state");
            }
        }
    }
}
```

#### 4.2 Parameter Control Integration
- Map WebRTC parameter commands to existing `WorldInterface` methods
- Support all physics parameters (gravity, elasticity, pressure, etc.)
- Real-time parameter updates with immediate effect

### Phase 5: Client Libraries and Examples (Week 3)

#### 5.1 JavaScript Client Library
```javascript
// webrtc-test-driver.js
class SparkeDuckTestDriver {
    constructor(signalingServerUrl = 'ws://localhost:8080') {
        this.pc = new RTCPeerConnection();
        this.dataChannel = null;
        this.pendingRequests = new Map();
    }
    
    async connect() { /* WebRTC connection logic */ }
    
    async step(count = 1) {
        return this.sendCommand('step', {count});
    }
    
    async getGridState() {
        return this.sendCommand('getState', {format: 'grid'});
    }
    
    async screenshot() {
        return this.sendCommand('screenshot', {format: 'png'});
    }
    
    onStateChanged(callback) { /* Event subscription */ }
}
```

#### 5.2 Python Client Library
```python
# sparkle_duck_driver.py
import asyncio
import json
from aiortc import RTCPeerConnection, RTCDataChannel

class SparkleDuckDriver:
    async def step(self, count: int = 1) -> dict:
        return await self._send_command('step', {'count': count})
    
    async def get_grid_state(self) -> dict:
        return await self._send_command('getState', {'format': 'grid'})
```

#### 5.3 Test Examples
```python
# Example: Automated test
async def test_gravity_simulation():
    driver = SparkleDuckDriver()
    await driver.connect()
    
    # Reset and setup
    await driver.reset()
    await driver.set_param('gravity', 9.81)
    await driver.add_material(50, 10, 'dirt', 1.0)
    
    # Run simulation
    for i in range(100):
        await driver.step()
        if i % 10 == 0:
            state = await driver.get_grid_state()
            # Validate dirt movement
            assert validate_gravity_behavior(state)
```

### Phase 6: Advanced Features (Week 4)

#### 6.1 Visual Streaming
- Continuous screenshot streaming for real-time monitoring
- Configurable FPS and quality settings
- Delta compression for bandwidth efficiency

#### 6.2 Batch Operations
- Queue multiple commands for atomic execution
- Transaction-like behavior for complex test scenarios
- Rollback capabilities using existing time reversal

#### 6.3 Performance Optimization
- Efficient JSON serialization for large grids
- WebRTC data channel flow control
- Client-side caching and state diffing

## File Structure

```
src/
├── webrtc/
│   ├── WebRTCManager.h/cpp           # Core WebRTC management
│   ├── WebRTCCommandHandler.h/cpp    # Command processing
│   ├── WebRTCStateSerializer.h/cpp   # Grid state serialization
│   └── WebRTCSignaling.h/cpp         # Signaling server
├── main.cpp                          # Add WebRTC initialization
└── lib/simulator_loop.h              # Add WebRTC integration

test_clients/
├── javascript/
│   ├── sparkle-duck-driver.js        # JS client library
│   └── examples/
├── python/
│   ├── sparkle_duck_driver.py        # Python client library
│   └── examples/
└── web_interface/
    ├── index.html                    # Web-based control panel
    └── dashboard.js
```

## Testing Strategy

### Unit Tests
- WebRTC message parsing and serialization
- Command handler logic with mock world interfaces
- State serialization accuracy across both physics systems

### Integration Tests
- End-to-end command execution through WebRTC
- Multi-client connection handling
- Performance testing with large grid states

### Visual Tests
- Screenshot capture accuracy
- Real-time streaming performance
- Cross-platform client compatibility

## Dependencies and Build Integration

### CMake Changes
```cmake
# Add WebRTC dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBDATACHANNEL REQUIRED libdatachannel)

# Add WebRTC sources
set(WEBRTC_SOURCES
    src/webrtc/WebRTCManager.cpp
    src/webrtc/WebRTCCommandHandler.cpp
    src/webrtc/WebRTCStateSerializer.cpp
    src/webrtc/WebRTCSignaling.cpp
)

# Update main executable
target_sources(sparkle-duck PRIVATE ${WEBRTC_SOURCES})
target_link_libraries(sparkle-duck ${LIBDATACHANNEL_LIBRARIES})
target_include_directories(sparkle-duck PRIVATE ${LIBDATACHANNEL_INCLUDE_DIRS})
```

### Runtime Dependencies
- libdatachannel (0.19.0+)
- OpenSSL (for DTLS)
- System ICE libraries

## Security Considerations

### Authentication
- Simple token-based authentication for production use
- CORS handling for web clients
- Rate limiting on command execution

### Sandboxing
- Command validation and parameter bounds checking
- Resource limits on state dumps and screenshots
- Client isolation for multi-user scenarios

## Future Enhancements

### Advanced Features
- Real-time physics parameter tuning with live visual feedback
- Record/replay functionality for test scenarios  
- Distributed testing across multiple simulation instances
- Integration with CI/CD pipelines for automated physics regression testing

### Performance Optimizations
- WebAssembly client libraries for better performance
- Binary protocols for high-frequency data
- GPU-accelerated state serialization

## Conclusion

This WebRTC test driver will provide a modern, real-time interface for controlling and monitoring the Sparkle Duck simulation. The implementation leverages existing infrastructure while adding powerful remote capabilities essential for automated testing and debugging workflows.

The phased approach ensures incremental development with testable milestones, allowing early validation of core concepts before building advanced features.
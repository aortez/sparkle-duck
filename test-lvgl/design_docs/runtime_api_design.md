# Runtime Communication API Design

## Recommended Approach: HTTP REST API + JSON

### **Libraries**
- **Message Passing**: [cpp-httplib](https://github.com/yhirose/cpp-httplib) (header-only HTTP server)
- **Serialization**: [nlohmann/json](https://github.com/nlohmann/json) (header-only JSON)

### **Installation**
```bash
# Download header-only libraries
wget https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h -O src/lib/httplib.h
wget https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp -O src/lib/json.hpp
```

## **API Specification**

### **Base URL**: `http://localhost:8080/api`

### **Core Simulation Control**

#### **GET /api/status**
Get current simulation state
```json
{
  "running": true,
  "paused": false,
  "timestep": 12345,
  "fps": 60,
  "total_mass": 123.45,
  "world_type": "WorldB",
  "physics": {
    "gravity": -9.8,
    "elasticity": 0.8,
    "pressure_system": "Original"
  }
}
```

#### **POST /api/pause**
Pause/unpause simulation
```json
Request: {"paused": true}
Response: {"success": true, "paused": true}
```

#### **POST /api/reset**
Reset simulation
```json
Request: {}
Response: {"success": true, "message": "Simulation reset"}
```

#### **POST /api/step**
Step simulation (when paused)
```json
Request: {"steps": 1}
Response: {"success": true, "timestep": 12346}
```

### **World Management**

#### **GET /api/world**
Get world information
```json
{
  "type": "WorldB",
  "width": 100,
  "height": 100,
  "total_cells": 10000,
  "active_cells": 1250
}
```

#### **POST /api/world/type**
Switch world type
```json
Request: {"type": "WorldA"}
Response: {"success": true, "switched_to": "WorldA"}
```

#### **GET /api/cells**
Get cell data (with optional filtering)
```json
Query: ?x=10&y=10&radius=5
Response: {
  "cells": [
    {"x": 10, "y": 10, "material": "DIRT", "mass": 1.5, "velocity": {"x": 0.1, "y": -0.2}},
    {"x": 11, "y": 10, "material": "WATER", "mass": 1.0, "velocity": {"x": 0.0, "y": -0.5}}
  ]
}
```

#### **POST /api/cells/{x}/{y}**
Modify specific cell
```json
Request: {"material": "WATER", "amount": 1.0}
Response: {"success": true, "cell": {"x": 10, "y": 10, "material": "WATER", "mass": 1.0}}
```

### **Physics Control**

#### **POST /api/physics/gravity**
Set gravity
```json
Request: {"value": -9.8}
Response: {"success": true, "gravity": -9.8}
```

#### **POST /api/physics/elasticity**
Set elasticity factor
```json
Request: {"value": 0.9}
Response: {"success": true, "elasticity": 0.9}
```

#### **POST /api/physics/pressure-system**
Change pressure system
```json
Request: {"system": "Iterative Settling"}
Response: {"success": true, "pressure_system": "Iterative Settling"}
```

### **Utility Functions**

#### **POST /api/screenshot**
Take screenshot
```json
Request: {"filename": "custom_name.png"}
Response: {"success": true, "filename": "screenshot_20241214_162000.png"}
```

#### **POST /api/material**
Add material at coordinates
```json
Request: {"x": 50, "y": 50, "material": "DIRT", "radius": 5}
Response: {"success": true, "cells_modified": 12}
```

### **Real-time Data Streaming**

#### **GET /api/stream/physics** (Server-Sent Events)
Stream physics data in real-time
```
data: {"timestep": 12345, "total_mass": 123.45, "fps": 60}

data: {"timestep": 12346, "total_mass": 123.50, "fps": 59}
```

## **Example Usage**

### **Command Line**
```bash
# Pause simulation
curl -X POST http://localhost:8080/api/pause -d '{"paused": true}'

# Add dirt at position
curl -X POST http://localhost:8080/api/material -d '{"x": 50, "y": 50, "material": "DIRT", "radius": 3}'

# Get simulation status
curl http://localhost:8080/api/status | jq .

# Take screenshot
curl -X POST http://localhost:8080/api/screenshot
```

### **Python Script**
```python
import requests
import json

base_url = "http://localhost:8080/api"

# Pause simulation
response = requests.post(f"{base_url}/pause", json={"paused": True})
print(response.json())

# Add materials in a pattern
for i in range(10):
    requests.post(f"{base_url}/material", json={
        "x": 50 + i, "y": 50, "material": "DIRT", "radius": 1
    })

# Get current state
status = requests.get(f"{base_url}/status").json()
print(f"Total mass: {status['total_mass']}")
```

### **JavaScript/Web**
```javascript
// Add material on click
async function addMaterial(x, y) {
    const response = await fetch('/api/material', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({x, y, material: 'WATER', radius: 2})
    });
    return response.json();
}

// Stream physics data
const eventSource = new EventSource('/api/stream/physics');
eventSource.onmessage = event => {
    const data = JSON.parse(event.data);
    updateUI(data);
};
```

## **Implementation Strategy**

### **Phase 1: Basic HTTP Server**
- Add cpp-httplib and nlohmann/json headers
- Create simple HTTP server thread
- Implement basic status endpoint

### **Phase 2: Core Commands**
- Pause/unpause, reset, step
- Physics parameter control
- Screenshot functionality

### **Phase 3: World Interaction**
- Cell data access and modification
- Material addition/removal
- World type switching

### **Phase 4: Real-time Features**
- Server-sent events for data streaming
- WebSocket support for bidirectional communication
- Performance monitoring endpoints

## **Security Considerations**

### **Local Only (Recommended)**
- Bind to localhost only: `server.listen("127.0.0.1", 8080)`
- No authentication needed for local development

### **Network Access (Optional)**
- Add API key authentication
- Rate limiting for commands
- Input validation and sanitization

## **Error Handling**

All endpoints return consistent error format:
```json
{
  "success": false,
  "error": "Invalid material type",
  "code": 400
}
```

## **Alternative Implementations**

### **Option 2: Unix Domain Socket + JSON**
```cpp
// Server side
int sock = socket(AF_UNIX, SOCK_STREAM, 0);
struct sockaddr_un addr;
addr.sun_family = AF_UNIX;
strcpy(addr.sun_path, "/tmp/sparkle-duck.sock");
bind(sock, (struct sockaddr*)&addr, sizeof(addr));

// Client usage
echo '{"command": "pause"}' | socat - UNIX-CONNECT:/tmp/sparkle-duck.sock
```

### **Option 3: ZeroMQ + MessagePack**
```cpp
// High-performance streaming
zmq::context_t context(1);
zmq::socket_t publisher(context, ZMQ_PUB);
publisher.bind("tcp://*:5556");

// Efficient binary serialization
msgpack::sbuffer buffer;
msgpack::pack(buffer, physics_data);
publisher.send(zmq::buffer(buffer.data(), buffer.size()));
```

## **Conclusion**

The HTTP REST API approach provides the best balance of:
- **Simplicity**: Standard HTTP + JSON
- **Compatibility**: Works with any HTTP client
- **Debuggability**: Human-readable protocol
- **Extensibility**: Easy to add new endpoints

Perfect for Sparkle Duck's needs of runtime control and monitoring.
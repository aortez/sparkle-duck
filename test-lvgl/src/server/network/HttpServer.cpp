#include "HttpServer.h"

#include <cpp-httplib/httplib.h>

#include <atomic>
#include <chrono>
#include <spdlog/spdlog.h>
#include <thread>

namespace DirtSim {
namespace Server {

struct HttpServer::Impl {
    std::atomic<bool> running_{ false };
    std::thread thread_;
    std::unique_ptr<httplib::Server> server_;

    void runServer(int port)
    {
        server_ = std::make_unique<httplib::Server>();

        server_->Get("/", [](const httplib::Request&, httplib::Response& res) {
            res.set_redirect("/garden");
        });

        server_->Get("/garden", [](const httplib::Request&, httplib::Response& res) {
            std::string html = R"HTML(<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <title>Dirt Sim Garden</title>
    <style>
        body { font-family: monospace; background: #1a1a1a; color: #00ff00; padding: 20px; }
        h1 { color: #00ff00; margin-bottom: 10px; }
        #status { margin: 10px 0; color: #ffff00; }
        .peers { margin-top: 20px; }
        .peer { border: 1px solid #00ff00; padding: 10px; margin: 10px 0; background: #0a0a0a; }
        .peer h3 { margin: 0 0 5px 0; color: #00ff00; }
        .peer-info { font-size: 12px; margin: 3px 0; }
        .debug-container { margin-top: 20px; }
        .debug-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 5px; }
        .debug-header h3 { color: #ff6600; margin: 0; }
        .debug-controls button { margin-left: 5px; padding: 3px 8px; background: #333; color: #ff6600; border: 1px solid #ff6600; cursor: pointer; font-family: monospace; font-size: 11px; }
        .debug-controls button:hover { background: #555; }
        .debug-controls button.active { background: #ff6600; color: #000; }
        #debug { padding: 10px; border: 1px solid #ff6600; background: #0a0a0a; font-size: 10px; color: #ff6600; height: 400px; overflow-y: auto; }
        #debug.paused { border-color: #ffaa00; }
    </style>
</head>
<body>
    <h1>Dirt Sim Garden</h1>
    <div id="status">Discovering...</div>
    <div class="peers" id="peers"></div>
    <div class="debug-container">
        <div class="debug-header">
            <h3>Debug Log</h3>
            <div class="debug-controls">
                <button id="pauseBtn" onclick="togglePause()">Pause</button>
                <button onclick="copyDebugLog(event)">Copy</button>
                <button onclick="clearDebugLog()">Clear</button>
            </div>
        </div>
        <div id="debug"></div>
    </div>
    <script>
        // Timestamp formatting with milliseconds.
        function formatTime(date) {
            return date.toLocaleTimeString() + '.' + date.getMilliseconds().toString().padStart(3, '0');
        }

        function formatElapsed(ms) {
            if (ms < 1000) return ms + 'ms';
            if (ms < 60000) return (ms / 1000).toFixed(1) + 's';
            return Math.floor(ms / 60000) + 'm ' + Math.floor((ms % 60000) / 1000) + 's';
        }

        // Debug log state.
        var debugPaused = false;

        // Debug log controls.
        function togglePause() {
            debugPaused = !debugPaused;
            var pauseBtn = document.getElementById('pauseBtn');
            var debug = document.getElementById('debug');
            if (debugPaused) {
                pauseBtn.textContent = 'Resume';
                pauseBtn.classList.add('active');
                debug.classList.add('paused');
            } else {
                pauseBtn.textContent = 'Pause';
                pauseBtn.classList.remove('active');
                debug.classList.remove('paused');
            }
        }

        function copyDebugLog(e) {
            var debug = document.getElementById('debug');
            var text = debug.innerText;
            navigator.clipboard.writeText(text).then(function() {
                var btn = e.target;
                var oldText = btn.textContent;
                btn.textContent = 'Copied!';
                setTimeout(function() { btn.textContent = oldText; }, 1000);
            }).catch(function(err) {
                alert('Failed to copy: ' + err);
            });
        }

        function clearDebugLog() {
            if (confirm('Clear debug log?')) {
                document.getElementById('debug').innerHTML = '';
            }
        }

        // Debug logging.
        function logDebug(msg) {
            if (debugPaused) return;

            var debug = document.getElementById('debug');
            var now = new Date();
            debug.innerHTML = '[' + formatTime(now) + '] ' + msg + '<br>' + debug.innerHTML;
            // Keep debug log from growing too large.
            var lines = debug.innerHTML.split('<br>');
            if (lines.length > 200) {
                debug.innerHTML = lines.slice(0, 200).join('<br>');
            }
        }

        function logResponse(cmdName, id, response) {
            var msg = cmdName + ' [id=' + id + ']: ';
            if (response.success === false || (response.error && !response.success)) {
                msg += 'ERROR: ' + response.error;
            } else if (response.success === true || response.value !== undefined || response.state !== undefined || response.pixels !== undefined) {
                msg += 'OK';
                var keys = Object.keys(response).filter(function(k) {
                    return k !== 'success' && k !== 'id' && k !== 'pixels';
                });
                if (keys.length > 0) {
                    msg += ' (' + keys.slice(0, 4).map(function(k) {
                        var v = response[k];
                        if (typeof v === 'string') return k + '="' + v.substring(0, 15) + (v.length > 15 ? '...' : '') + '"';
                        if (typeof v === 'object') return k + '={...}';
                        return k + '=' + v;
                    }).join(', ') + ')';
                }
                if (response.pixels) {
                    msg += ' +pixels[' + response.pixels.length + ' bytes]';
                }
            } else {
                msg += 'UNKNOWN FORMAT: ' + JSON.stringify(response).substring(0, 50);
            }
            logDebug(msg);
        }

        // Global state tracking.
        var globalState = {
            lastUpdate: null,
            serverLastResponse: null,
            uiLastResponse: null
        };

        // Update the status display with elapsed time.
        function updateStatusDisplay() {
            var status = document.getElementById('status');
            var parts = [];

            parts.push('Server: ' + (serverConn.isConnected() ? 'connected' : 'disconnected'));
            if (globalState.serverLastResponse) {
                var elapsed = Date.now() - globalState.serverLastResponse.getTime();
                parts.push('last: ' + formatElapsed(elapsed) + ' ago');
            }

            parts.push('| UI: ' + (uiConn.isConnected() ? 'connected' : 'disconnected'));
            if (globalState.uiLastResponse) {
                var elapsed = Date.now() - globalState.uiLastResponse.getTime();
                parts.push('last: ' + formatElapsed(elapsed) + ' ago');
            }

            if (globalState.lastUpdate) {
                parts.push('| Updated: ' + formatTime(globalState.lastUpdate));
            }

            status.textContent = parts.join(' ');
        }

        // Refresh status display every second.
        setInterval(updateStatusDisplay, 1000);

        // Persistent WebSocket connection manager.
        function createPersistentConnection(name, url, onStatusChange) {
            var conn = {
                name: name,
                url: url,
                socket: null,
                pendingRequests: {},
                connected: false,
                reconnectDelay: 1000,
                maxReconnectDelay: 30000,
                reconnectTimer: null,
                lastResponse: null
            };

            function connect() {
                if (conn.socket && conn.socket.readyState === WebSocket.OPEN) {
                    return;
                }

                logDebug(name + ': Connecting to ' + url);
                conn.socket = new WebSocket(url);

                conn.socket.onopen = function() {
                    logDebug(name + ': Connected (persistent)');
                    conn.connected = true;
                    conn.reconnectDelay = 1000;
                    if (onStatusChange) onStatusChange(true);
                    updateStatusDisplay();
                };

                conn.socket.onmessage = function(event) {
                    var processMessage = function(text) {
                        try {
                            var response = JSON.parse(text);
                            conn.lastResponse = new Date();
                            if (name === 'Server') globalState.serverLastResponse = conn.lastResponse;
                            if (name === 'UI') globalState.uiLastResponse = conn.lastResponse;

                            // Match response to pending request by ID.
                            if (response.id && conn.pendingRequests[response.id]) {
                                var req = conn.pendingRequests[response.id];
                                delete conn.pendingRequests[response.id];
                                clearTimeout(req.timeoutId);
                                logResponse(req.cmdName, response.id, response);
                                if (req.onSuccess && response.success !== false) {
                                    req.onSuccess(response);
                                }
                            } else if (response.id) {
                                logDebug(name + ': Stale response [id=' + response.id + '] (no pending request)');
                            }
                        } catch (e) {
                            logDebug(name + ': JSON parse error: ' + e.message + ' (len=' + text.length + ')');
                        }
                    };

                    if (event.data instanceof Blob) {
                        logDebug(name + ': Receiving Blob (' + event.data.size + ' bytes)...');
                        event.data.text().then(processMessage).catch(function(e) {
                            logDebug(name + ': Blob decode error: ' + e.message);
                        });
                    } else {
                        processMessage(event.data);
                    }
                };

                conn.socket.onerror = function(e) {
                    logDebug(name + ': WebSocket error: ' + (e.message || 'unknown'));
                };

                conn.socket.onclose = function(e) {
                    logDebug(name + ': Disconnected (code=' + e.code + ', reason=' + (e.reason || 'none') + ')');
                    conn.connected = false;
                    conn.socket = null;
                    if (onStatusChange) onStatusChange(false);
                    updateStatusDisplay();

                    // Fail any pending requests.
                    var pendingIds = Object.keys(conn.pendingRequests);
                    for (var i = 0; i < pendingIds.length; i++) {
                        var id = pendingIds[i];
                        var req = conn.pendingRequests[id];
                        clearTimeout(req.timeoutId);
                        logDebug(name + ': Request ' + req.cmdName + ' [id=' + id + '] failed (disconnected)');
                    }
                    conn.pendingRequests = {};

                    // Schedule reconnect with exponential backoff.
                    logDebug(name + ': Reconnecting in ' + (conn.reconnectDelay / 1000) + 's...');
                    conn.reconnectTimer = setTimeout(function() {
                        connect();
                    }, conn.reconnectDelay);
                    conn.reconnectDelay = Math.min(conn.reconnectDelay * 2, conn.maxReconnectDelay);
                };
            }

            conn.send = function(cmdName, params, onSuccess, timeoutMs) {
                if (!conn.socket || conn.socket.readyState !== WebSocket.OPEN) {
                    logDebug(name + ': Cannot send ' + cmdName + ' - not connected');
                    return false;
                }

                var id = Math.floor(Math.random() * 1000000);
                var cmdObj = { command: cmdName, id: id };
                if (params) {
                    Object.assign(cmdObj, params);
                }

                // Set up timeout for this request.
                var timeoutId = setTimeout(function() {
                    if (conn.pendingRequests[id]) {
                        delete conn.pendingRequests[id];
                        logDebug(name + ': ' + cmdName + ' [id=' + id + '] TIMEOUT after ' + (timeoutMs || 10000) + 'ms');
                    }
                }, timeoutMs || 10000);

                conn.pendingRequests[id] = {
                    cmdName: cmdName,
                    onSuccess: onSuccess,
                    timeoutId: timeoutId,
                    sentAt: new Date()
                };

                logDebug(name + ': Sending ' + cmdName + ' [id=' + id + ']');
                conn.socket.send(JSON.stringify(cmdObj));
                return true;
            };

            conn.isConnected = function() {
                return conn.connected && conn.socket && conn.socket.readyState === WebSocket.OPEN;
            };

            conn.connect = connect;

            // Start connecting.
            connect();

            return conn;
        }

        // Create persistent connections to server and UI.
        var serverConn = createPersistentConnection(
            'Server',
            'ws://' + window.location.hostname + ':8080',
            function(connected) { updateStatusDisplay(); }
        );

        var uiConn = createPersistentConnection(
            'UI',
            'ws://' + window.location.hostname + ':7070',
            function(connected) { updateStatusDisplay(); }
        );

        // Display peer information.
        function displayPeers(peers) {
            globalState.lastUpdate = new Date();
            var container = document.getElementById('peers');
            container.innerHTML = '';

            // Always add localhost peers first (not advertised via mDNS).
            var allPeers = [
                { name: 'Local Physics Server', host: 'localhost', port: 8080, role: 'physics' },
                { name: 'Local UI', host: 'localhost', port: 7070, role: 'ui' }
            ];

            // Add discovered remote peers.
            for (var i = 0; i < peers.length; i++) {
                allPeers.push(peers[i]);
            }

            for (var i = 0; i < allPeers.length; i++) {
                var peer = allPeers[i];
                var div = document.createElement('div');
                div.className = 'peer';
                div.id = 'peer-' + peer.host + '-' + peer.port;

                var conn = (peer.port === 8080) ? serverConn : uiConn;
                var connStatus = conn.isConnected() ? 'connected' : 'disconnected';

                var html = '<h3>' + peer.name + ' <small style="color:#888">(' + connStatus + ')</small></h3>' +
                    '<div class="peer-info">Host: ' + peer.host + ':' + peer.port + '</div>' +
                    '<div class="peer-info">Role: ' + peer.role + '</div>' +
                    '<div class="peer-info">State: <span id="state-' + peer.host + '-' + peer.port + '">...</span></div>';

                if (peer.role === 'ui') {
                    html += '<div class="peer-info">Screenshot: <span id="screenshot-status-' + peer.host + '-' + peer.port + '">waiting...</span></div>';
                    html += '<canvas id="screenshot-' + peer.host + '-' + peer.port + '" style="max-width: 320px; border: 1px solid #00ff00; margin-top: 10px;"></canvas>';
                }

                div.innerHTML = html;
                container.appendChild(div);

                // Query status using persistent connections.
                queryStatus(peer);
            }

            updateStatusDisplay();
        }

        function queryStatus(peer) {
            var conn = (peer.port === 8080) ? serverConn : uiConn;
            conn.send('status_get', null, function(response) {
                var stateSpan = document.getElementById('state-' + peer.host + '-' + peer.port);
                if (stateSpan) {
                    var state = 'Unknown';
                    if (response.state) {
                        state = response.state;
                    } else if (response.value) {
                        if (response.value.scenario_id) {
                            state = 'Running (' + response.value.scenario_id + ')';
                        } else if (response.value.width !== undefined) {
                            state = 'Idle';
                        }
                    }
                    stateSpan.textContent = state;
                }
            });
        }

        function discoverPeers() {
            serverConn.send('peers_get', null, function(response) {
                if (response.value && response.value.peers) {
                    displayPeers(response.value.peers);
                }
            });
        }

        // Screenshot loop using persistent UI connection.
        var screenshotInterval = null;
        var lastScreenshotTime = null;

        function startScreenshotLoop() {
            if (screenshotInterval) return;

            function fetchScreenshot() {
                var statusSpan = document.getElementById('screenshot-status-localhost-7070');

                if (!uiConn.isConnected()) {
                    if (statusSpan) statusSpan.textContent = 'UI not connected';
                    return;
                }

                if (statusSpan) statusSpan.textContent = 'requesting...';

                uiConn.send('screen_grab', { scale: 0.25 }, function(response) {
                    if (response.pixels) {
                        lastScreenshotTime = new Date();
                        var canvas = document.getElementById('screenshot-localhost-7070');
                        if (canvas) {
                            var ctx = canvas.getContext('2d');
                            canvas.width = response.width;
                            canvas.height = response.height;
                            var pixelData = atob(response.pixels);
                            var imageData = ctx.createImageData(response.width, response.height);
                            // Convert ARGB8888 (B,G,R,A in little-endian) to RGBA for canvas.
                            for (var i = 0; i < pixelData.length; i += 4) {
                                imageData.data[i + 0] = pixelData.charCodeAt(i + 2); // R
                                imageData.data[i + 1] = pixelData.charCodeAt(i + 1); // G
                                imageData.data[i + 2] = pixelData.charCodeAt(i + 0); // B
                                imageData.data[i + 3] = pixelData.charCodeAt(i + 3); // A
                            }
                            ctx.putImageData(imageData, 0, 0);
                        }
                        if (statusSpan) statusSpan.textContent = response.width + 'x' + response.height + ' @ ' + formatTime(lastScreenshotTime);
                    } else if (response.error) {
                        if (statusSpan) statusSpan.textContent = 'error: ' + response.error;
                    }
                }, 8000);
            }

            // Fetch screenshots every 2 seconds (server throttles to 1/sec anyway).
            screenshotInterval = setInterval(fetchScreenshot, 2000);
            // Also fetch one immediately.
            setTimeout(fetchScreenshot, 500);
        }

        // Start discovery once connections are ready.
        setTimeout(function() {
            discoverPeers();
            startScreenshotLoop();
        }, 1000);

        // Refresh peer list periodically.
        setInterval(discoverPeers, 5000);
    </script>
</body>
</html>)HTML";
            res.set_content(html, "text/html");
        });

        spdlog::info("HttpServer: Starting on port {}", port);
        spdlog::info("HttpServer: Dashboard available at http://localhost:{}/garden", port);

        running_ = true;
        if (!server_->listen("0.0.0.0", port)) {
            spdlog::error("HttpServer: Failed to start on port {}", port);
            running_ = false;
        }
    }
};

HttpServer::HttpServer(int port) : pImpl_(std::make_unique<Impl>()), port_(port)
{}

HttpServer::~HttpServer()
{
    stop();
}

bool HttpServer::start()
{
    if (pImpl_->running_) {
        return true;
    }

    pImpl_->thread_ = std::thread([this]() { pImpl_->runServer(port_); });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return pImpl_->running_;
}

void HttpServer::stop()
{
    if (!pImpl_->running_) {
        return;
    }

    if (pImpl_->server_) {
        pImpl_->server_->stop();
    }

    if (pImpl_->thread_.joinable()) {
        pImpl_->thread_.join();
    }

    pImpl_->running_ = false;
    spdlog::info("HttpServer: Stopped");
}

bool HttpServer::isRunning() const
{
    return pImpl_->running_;
}

} // namespace Server
} // namespace DirtSim

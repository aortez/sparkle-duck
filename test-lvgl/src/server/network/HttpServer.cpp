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
            std::string html = R"(<!DOCTYPE html>
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
        #debug { margin-top: 20px; padding: 10px; border: 1px solid #ff6600; background: #0a0a0a; font-size: 10px; color: #ff6600; max-height: 200px; overflow-y: auto; }
    </style>
</head>
<body>
    <h1>Dirt Sim Garden</h1>
    <div id="status">Discovering...</div>
    <div class="peers" id="peers"></div>
    <div id="debug"></div>
    <script>
        function logDebug(msg) {
            var debug = document.getElementById('debug');
            var now = new Date();
            var time = now.toLocaleTimeString() + '.' + now.getMilliseconds().toString().padStart(3, '0');
            debug.innerHTML = '[' + time + '] ' + msg + '<br>' + debug.innerHTML;
        }
        function discoverPeers() {
            logDebug('Discovering peers...');
            var ws = new WebSocket('ws://' + window.location.hostname + ':8080');

            ws.onopen = function() {
                logDebug('Sending peers_get');
                ws.send(JSON.stringify({ command: 'peers_get', id: 1 }));
            };

            ws.onmessage = function(event) {
                var processMessage = function(text) {
                    try {
                        var response = JSON.parse(text);
                        // Only process messages with 'id' field (ignore WorldData broadcasts).
                        if (!response.id) {
                            return;
                        }
                        if (response.value && response.value.peers) {
                            displayPeers(response.value.peers);
                        }
                        ws.close();
                    } catch (e) {
                        // Ignore binary WorldData or malformed messages.
                    }
                };

                if (event.data instanceof Blob) {
                    event.data.text().then(processMessage);
                } else {
                    processMessage(event.data);
                }
            };

            ws.onerror = function() {
                document.getElementById('status').textContent = 'Error connecting to server';
            };
        }

        function displayPeers(peers) {
            var status = document.getElementById('status');
            var container = document.getElementById('peers');
            container.innerHTML = '';

            if (peers.length === 0) {
                status.textContent = 'No simulations found';
                return;
            }

            var now = new Date().toLocaleTimeString();
            status.textContent = 'Found ' + peers.length + ' simulation(s) (updated: ' + now + ')';

            for (var i = 0; i < peers.length; i++) {
                var peer = peers[i];
                var div = document.createElement('div');
                div.className = 'peer';
                div.id = 'peer-' + peer.host + '-' + peer.port;
                div.innerHTML = '<h3>' + peer.name + '</h3>' +
                    '<div class="peer-info">Host: ' + peer.host + '</div>' +
                    '<div class="peer-info">Port: ' + peer.port + '</div>' +
                    '<div class="peer-info">Role: ' + peer.role + '</div>' +
                    '<div class="peer-info">State: <span id="state-' + peer.host + '-' + peer.port + '">...</span></div>';
                container.appendChild(div);

                // Query status for both physics and UI peers.
                queryStatus(peer);
            }
        }

        function queryStatus(peer) {
            var addr = peer.host || peer.address;

            if (addr.indexOf('fe80::') === 0) {
                logDebug('Skipping IPv6 link-local address: ' + addr);
                var stateSpan = document.getElementById('state-' + peer.host + '-' + peer.port);
                if (stateSpan) {
                    stateSpan.textContent = 'IPv6 unsupported';
                }
                return;
            }

            if (addr.indexOf(':') !== -1 && addr.indexOf('[') === -1) {
                addr = '[' + addr + ']';
            }
            logDebug('Querying status for ' + addr + ':' + peer.port);
            var statusWs = new WebSocket('ws://' + addr + ':' + peer.port);

            statusWs.onopen = function() {
                logDebug('Sending status_get to ' + peer.host);
                statusWs.send(JSON.stringify({ command: 'status_get', id: 1 }));
            };
            statusWs.onmessage = function(event) {
                var processMessage = function(text) {
                    try {
                        var response = JSON.parse(text);
                        // Only process messages with 'id' field (ignore WorldData broadcasts).
                        if (!response.id) {
                            return;
                        }
                        var stateSpan = document.getElementById('state-' + peer.host + '-' + peer.port);
                        if (stateSpan) {
                            var state = 'Unknown';
                            // UI response format: has .state at top level.
                            if (response.state) {
                                state = response.state;
                            }
                            // Physics response format: has .value.scenario_id or .value.width.
                            else if (response.value) {
                                if (response.value.scenario_id) {
                                    state = 'Running (' + response.value.scenario_id + ', step ' + response.value.timestep + ')';
                                } else if (response.value.width !== undefined) {
                                    state = 'Idle (ready)';
                                }
                            }
                            stateSpan.textContent = state;
                        }
                        statusWs.close();
                    } catch (e) {
                        // Ignore binary WorldData or malformed messages.
                    }
                };

                if (event.data instanceof Blob) {
                    event.data.text().then(processMessage);
                } else {
                    processMessage(event.data);
                }
            };
        }

        discoverPeers();
        setInterval(discoverPeers, 5000);
    </script>
</body>
</html>)";
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

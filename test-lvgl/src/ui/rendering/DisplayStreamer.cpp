#include "DisplayStreamer.h"

#include <algorithm>
#include <rtc/rtc.hpp>
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {

DisplayStreamer::DisplayStreamer()
{
    spdlog::debug("DisplayStreamer created");
}

DisplayStreamer::~DisplayStreamer()
{
    std::lock_guard<std::mutex> lock(clientsMutex_);
    clients_.clear();
    spdlog::debug("DisplayStreamer destroyed");
}

void DisplayStreamer::setDisplay(lv_display_t* display)
{
    display_ = display;
    spdlog::info("DisplayStreamer: Display set");
}

void DisplayStreamer::tryCapture()
{
    if (!display_) {
        return;
    }

    std::lock_guard<std::mutex> lock(clientsMutex_);

    if (clients_.empty()) {
        return;
    }

    auto now = std::chrono::steady_clock::now();

    for (auto& client : clients_) {
        auto elapsed = now - client.last_frame_time;

        if (elapsed >= client.frameInterval()) {
            auto frame = FramebufferCapture::capture(display_, client.format);

            if (frame && frame->data.size() > 0) {
                try {
                    const auto* data_ptr = reinterpret_cast<const std::byte*>(frame->data.data());
                    client.ws->send(data_ptr, frame->data.size());
                    client.last_frame_time = now;

                    spdlog::debug(
                        "DisplayStreamer: Sent frame ({} bytes, format: {})",
                        frame->data.size(),
                        client.format == ImageFormat::PNG ? "PNG" : "JPEG");
                }
                catch (const std::exception& e) {
                    spdlog::warn("DisplayStreamer: Failed to send frame: {}", e.what());
                }
            }
        }
    }
}

void DisplayStreamer::addClient(std::shared_ptr<rtc::WebSocket> ws, int fps, ImageFormat format)
{
    std::lock_guard<std::mutex> lock(clientsMutex_);

    StreamClient client;
    client.ws = ws;
    client.target_fps = fps;
    client.format = format;
    client.last_frame_time = std::chrono::steady_clock::now();

    clients_.push_back(client);

    spdlog::info(
        "DisplayStreamer: Added client (fps={}, format={}, total clients={})",
        fps,
        format == ImageFormat::PNG ? "PNG" : "JPEG",
        clients_.size());
}

void DisplayStreamer::removeClient(std::shared_ptr<rtc::WebSocket> ws)
{
    std::lock_guard<std::mutex> lock(clientsMutex_);

    auto it = std::remove_if(
        clients_.begin(), clients_.end(), [&ws](const StreamClient& c) { return c.ws == ws; });

    if (it != clients_.end()) {
        clients_.erase(it, clients_.end());
        spdlog::info("DisplayStreamer: Removed client (remaining clients={})", clients_.size());
    }
}

bool DisplayStreamer::hasClients() const
{
    std::lock_guard<std::mutex> lock(clientsMutex_);
    return !clients_.empty();
}

size_t DisplayStreamer::clientCount() const
{
    std::lock_guard<std::mutex> lock(clientsMutex_);
    return clients_.size();
}

} // namespace Ui
} // namespace DirtSim

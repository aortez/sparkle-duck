#pragma once

#include "FramebufferCapture.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <vector>

namespace rtc {
class WebSocket;
}

typedef struct _lv_display_t lv_display_t;

namespace DirtSim {
namespace Ui {

struct StreamClient {
    std::shared_ptr<rtc::WebSocket> ws;
    int target_fps;
    ImageFormat format;
    std::chrono::steady_clock::time_point last_frame_time;

    std::chrono::milliseconds frameInterval() const
    {
        return std::chrono::milliseconds(1000 / target_fps);
    }
};

class DisplayStreamer {
public:
    DisplayStreamer();
    ~DisplayStreamer();

    void setDisplay(lv_display_t* display);
    void tryCapture();

    void addClient(
        std::shared_ptr<rtc::WebSocket> ws, int fps, ImageFormat format = ImageFormat::JPEG);
    void removeClient(std::shared_ptr<rtc::WebSocket> ws);
    bool hasClients() const;
    size_t clientCount() const;

private:
    lv_display_t* display_ = nullptr;
    mutable std::mutex clientsMutex_;
    std::vector<StreamClient> clients_;
};

} // namespace Ui
} // namespace DirtSim

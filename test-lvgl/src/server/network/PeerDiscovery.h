#pragma once

#include "core/Pimpl.h"

#include <functional>
#include <string>
#include <vector>

namespace DirtSim {
namespace Server {

enum class PeerRole { Physics, Ui, Unknown };

struct PeerInfo {
    std::string name;
    std::string host;
    std::string address;
    uint16_t port = 0;
    PeerRole role = PeerRole::Unknown;

    bool operator==(const PeerInfo& other) const
    {
        return name == other.name && host == other.host && port == other.port;
    }
};

class PeerDiscovery {
public:
    PeerDiscovery();
    ~PeerDiscovery();

    PeerDiscovery(const PeerDiscovery&) = delete;
    PeerDiscovery& operator=(const PeerDiscovery&) = delete;

    bool start();
    void stop();
    bool isRunning() const;

    std::vector<PeerInfo> getPeers() const;
    void setOnPeersChanged(std::function<void(const std::vector<PeerInfo>&)> callback);

private:
    struct Impl;
    Pimpl<Impl> pImpl_;
};

} // namespace Server
} // namespace DirtSim

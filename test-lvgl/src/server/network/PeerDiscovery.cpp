#include "PeerDiscovery.h"

#include <atomic>
#include <mutex>
#include <thread>

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/simple-watch.h>
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Server {

struct PeerDiscovery::Impl {
    std::atomic<bool> running_{ false };
    std::thread thread_;
    mutable std::mutex mutex_;
    std::vector<PeerInfo> peers_;
    std::function<void(const std::vector<PeerInfo>&)> onPeersChanged_;

    AvahiSimplePoll* poll_ = nullptr;
    AvahiClient* client_ = nullptr;
    AvahiServiceBrowser* browser_ = nullptr;

    static void clientCallback(AvahiClient* client, AvahiClientState state, void* userdata)
    {
        auto* self = static_cast<Impl*>(userdata);

        if (state == AVAHI_CLIENT_FAILURE) {
            spdlog::error(
                "PeerDiscovery: Avahi client failure: {}",
                avahi_strerror(avahi_client_errno(client)));
            avahi_simple_poll_quit(self->poll_);
        }
    }

    static void browseCallback(
        AvahiServiceBrowser* /*browser*/,
        AvahiIfIndex interface,
        AvahiProtocol protocol,
        AvahiBrowserEvent event,
        const char* name,
        const char* type,
        const char* domain,
        AvahiLookupResultFlags /*flags*/,
        void* userdata)
    {
        auto* self = static_cast<Impl*>(userdata);

        switch (event) {
            case AVAHI_BROWSER_NEW:
                spdlog::debug("PeerDiscovery: Found service '{}', resolving...", name);
                avahi_service_resolver_new(
                    self->client_,
                    interface,
                    protocol,
                    name,
                    type,
                    domain,
                    AVAHI_PROTO_UNSPEC,
                    static_cast<AvahiLookupFlags>(0),
                    resolveCallback,
                    userdata);
                break;

            case AVAHI_BROWSER_REMOVE:
                spdlog::debug("PeerDiscovery: Service '{}' removed", name);
                self->removePeer(name);
                break;

            case AVAHI_BROWSER_FAILURE:
                spdlog::error(
                    "PeerDiscovery: Browser failure: {}",
                    avahi_strerror(avahi_client_errno(self->client_)));
                avahi_simple_poll_quit(self->poll_);
                break;

            case AVAHI_BROWSER_ALL_FOR_NOW:
            case AVAHI_BROWSER_CACHE_EXHAUSTED:
                break;
        }
    }

    static void resolveCallback(
        AvahiServiceResolver* resolver,
        AvahiIfIndex /*interface*/,
        AvahiProtocol /*protocol*/,
        AvahiResolverEvent event,
        const char* name,
        const char* /*type*/,
        const char* /*domain*/,
        const char* hostName,
        const AvahiAddress* address,
        uint16_t port,
        AvahiStringList* txt,
        AvahiLookupResultFlags /*flags*/,
        void* userdata)
    {
        auto* self = static_cast<Impl*>(userdata);

        if (event == AVAHI_RESOLVER_FOUND) {
            char addrStr[AVAHI_ADDRESS_STR_MAX];
            avahi_address_snprint(addrStr, sizeof(addrStr), address);

            PeerInfo peer;
            peer.name = name;
            peer.host = hostName;
            peer.address = addrStr;
            peer.port = port;
            peer.role = PeerRole::Unknown;

            // Parse TXT records for role.
            for (AvahiStringList* l = txt; l != nullptr; l = avahi_string_list_get_next(l)) {
                char* key = nullptr;
                char* value = nullptr;
                if (avahi_string_list_get_pair(l, &key, &value, nullptr) == 0) {
                    if (key && value && std::string(key) == "role") {
                        if (std::string(value) == "physics") {
                            peer.role = PeerRole::Physics;
                        }
                        else if (std::string(value) == "ui") {
                            peer.role = PeerRole::Ui;
                        }
                    }
                    avahi_free(key);
                    avahi_free(value);
                }
            }

            spdlog::info("PeerDiscovery: Resolved '{}' at {}:{}", name, peer.host, port);
            self->addPeer(peer);
        }
        else if (event == AVAHI_RESOLVER_FAILURE) {
            spdlog::warn(
                "PeerDiscovery: Failed to resolve '{}': {}",
                name,
                avahi_strerror(avahi_client_errno(self->client_)));
        }

        avahi_service_resolver_free(resolver);
    }

    void addPeer(const PeerInfo& peer)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::find(peers_.begin(), peers_.end(), peer);
        if (it == peers_.end()) {
            peers_.push_back(peer);
            if (onPeersChanged_) {
                onPeersChanged_(peers_);
            }
        }
    }

    void removePeer(const std::string& name)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::remove_if(
            peers_.begin(), peers_.end(), [&name](const PeerInfo& p) { return p.name == name; });
        if (it != peers_.end()) {
            peers_.erase(it, peers_.end());
            if (onPeersChanged_) {
                onPeersChanged_(peers_);
            }
        }
    }

    bool startAvahi()
    {
        poll_ = avahi_simple_poll_new();
        if (!poll_) {
            spdlog::error("PeerDiscovery: Failed to create Avahi simple poll.");
            return false;
        }

        int error = 0;
        client_ = avahi_client_new(
            avahi_simple_poll_get(poll_),
            static_cast<AvahiClientFlags>(0),
            clientCallback,
            this,
            &error);
        if (!client_) {
            spdlog::error(
                "PeerDiscovery: Failed to create Avahi client: {}", avahi_strerror(error));
            avahi_simple_poll_free(poll_);
            poll_ = nullptr;
            return false;
        }

        browser_ = avahi_service_browser_new(
            client_,
            AVAHI_IF_UNSPEC,
            AVAHI_PROTO_UNSPEC,
            "_sparkle-duck._tcp",
            nullptr,
            static_cast<AvahiLookupFlags>(0),
            browseCallback,
            this);
        if (!browser_) {
            spdlog::error(
                "PeerDiscovery: Failed to create service browser: {}",
                avahi_strerror(avahi_client_errno(client_)));
            avahi_client_free(client_);
            avahi_simple_poll_free(poll_);
            client_ = nullptr;
            poll_ = nullptr;
            return false;
        }

        spdlog::info("PeerDiscovery: Started browsing for _sparkle-duck._tcp services.");
        return true;
    }

    void stopAvahi()
    {
        if (browser_) {
            avahi_service_browser_free(browser_);
            browser_ = nullptr;
        }
        if (client_) {
            avahi_client_free(client_);
            client_ = nullptr;
        }
        if (poll_) {
            avahi_simple_poll_free(poll_);
            poll_ = nullptr;
        }
    }

    void runLoop()
    {
        if (!startAvahi()) {
            running_ = false;
            return;
        }

        while (running_) {
            int result = avahi_simple_poll_iterate(poll_, 100);
            if (result != 0) {
                break;
            }
        }

        stopAvahi();
        spdlog::info("PeerDiscovery: Stopped.");
    }
};

PeerDiscovery::PeerDiscovery() : pImpl_()
{}

PeerDiscovery::~PeerDiscovery()
{
    stop();
}

bool PeerDiscovery::start()
{
    if (pImpl_->running_) {
        return true;
    }

    pImpl_->running_ = true;
    pImpl_->thread_ = std::thread([this]() { pImpl_->runLoop(); });
    return true;
}

void PeerDiscovery::stop()
{
    if (!pImpl_->running_) {
        return;
    }

    pImpl_->running_ = false;
    if (pImpl_->poll_) {
        avahi_simple_poll_quit(pImpl_->poll_);
    }
    if (pImpl_->thread_.joinable()) {
        pImpl_->thread_.join();
    }
}

bool PeerDiscovery::isRunning() const
{
    return pImpl_->running_;
}

std::vector<PeerInfo> PeerDiscovery::getPeers() const
{
    std::lock_guard<std::mutex> lock(pImpl_->mutex_);
    return pImpl_->peers_;
}

void PeerDiscovery::setOnPeersChanged(std::function<void(const std::vector<PeerInfo>&)> callback)
{
    std::lock_guard<std::mutex> lock(pImpl_->mutex_);
    pImpl_->onPeersChanged_ = std::move(callback);
}

} // namespace Server
} // namespace DirtSim

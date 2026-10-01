//
// Created on 2026/09/25.
//

#include "LoopbackBackend.h"

#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace sky::net::test {

    namespace {

        struct LoopbackItem {
            ChannelId            channel  = 0;
            MessageSequence      sequence = 0;
            std::vector<uint8_t> payload;
        };

        struct LoopbackConnImpl;

        struct LoopbackConnImpl : public INetConnection {
            ConnectionId      id;
            uint64_t          key             = 0;
            uint64_t          peerKey         = 0;
            LoopbackBackend  *owner           = nullptr;
            bool              connectedEmitted = false;
            bool              discPending     = false;
            bool              discEmitted     = false;
            bool              closed          = false;
            DisconnectReason  discReason      = DisconnectReason::Unknown;
            uint32_t          seq             = 0;
            uint32_t          rttMs           = 0;
            std::deque<LoopbackItem> inbox;

            ConnectionId GetId() const override { return id; }
            NetResult Send(ChannelId channel, std::span<const uint8_t> payload, DeliveryMode mode) override;
            void Close(DisconnectReason reason) override;
            uint32_t GetRttMs() const override { return rttMs; }
        };

        struct LoopbackListenerImpl : public INetListener {
            LoopbackBackend *owner = nullptr;
            NetworkAddress   address;
            bool             closed = false;

            void Broadcast(ChannelId channel, std::span<const uint8_t> payload, DeliveryMode mode) override;
            NetworkAddress GetAddress() const override { return address; }
            void Close() override;
        };

        struct LoopbackBus {
            std::mutex mutex;

            struct BackendEntry {
                std::vector<uint64_t>      connKeys;
                LoopbackListenerImpl      *listener = nullptr;
            };

            std::unordered_map<LoopbackBackend *, BackendEntry>   backends;
            std::unordered_map<std::string, LoopbackBackend *>    listeners;
            std::unordered_map<uint64_t, LoopbackConnImpl *>      conns;
            uint64_t nextKey       = 1;
            uint64_t nextConnIndex = 1;

            static LoopbackBus &Get()
            {
                static LoopbackBus bus;
                return bus;
            }
        };

        LoopbackConnImpl *FindPeer(LoopbackBus &bus, LoopbackConnImpl *conn)
        {
            if (conn == nullptr || conn->peerKey == 0) {
                return nullptr;
            }
            auto it = bus.conns.find(conn->peerKey);
            return it == bus.conns.end() ? nullptr : it->second;
        }

        NetResult LoopbackConnImpl::Send(ChannelId channel, std::span<const uint8_t> payload, DeliveryMode mode)
        {
            auto &bus = LoopbackBus::Get();
            std::lock_guard<std::mutex> lock(bus.mutex);

            if (closed || peerKey == 0) {
                return NetResult::NotConnected;
            }
            LoopbackConnImpl *peer = FindPeer(bus, this);
            if (peer == nullptr || peer->closed) {
                return NetResult::NotConnected;
            }
            if (owner != nullptr) {
                const bool unreliable = mode == DeliveryMode::Unreliable || mode == DeliveryMode::UnreliableSequenced;
                if (unreliable && owner->ShouldDropNextSend()) {
                    return NetResult::Ok;
                }
            }

            LoopbackItem item;
            item.channel  = channel;
            item.sequence = seq++;
            item.payload.assign(payload.begin(), payload.end());
            peer->inbox.push_back(std::move(item));
            return NetResult::Ok;
        }

        void LoopbackConnImpl::Close(DisconnectReason reason)
        {
            auto &bus = LoopbackBus::Get();
            std::lock_guard<std::mutex> lock(bus.mutex);

            if (closed) {
                return;
            }
            closed = true;
            LoopbackConnImpl *peer = FindPeer(bus, this);
            if (peer != nullptr && !peer->closed) {
                peer->discPending = true;
                if (!peer->discEmitted) {
                    peer->discReason = reason == DisconnectReason::LocalClose ? DisconnectReason::RemoteClose : reason;
                }
            }
            peerKey = 0;
            if (peer != nullptr) {
                peer->peerKey = 0;
            }
        }

        void LoopbackListenerImpl::Broadcast(ChannelId channel, std::span<const uint8_t> payload, DeliveryMode mode)
        {
            auto &bus = LoopbackBus::Get();
            std::vector<uint64_t> keys;
            {
                std::lock_guard<std::mutex> lock(bus.mutex);
                auto it = bus.backends.find(owner);
                if (it == bus.backends.end()) {
                    return;
                }
                keys = it->second.connKeys;
            }
            for (auto key : keys) {
                LoopbackConnImpl *conn = nullptr;
                {
                    std::lock_guard<std::mutex> lock(bus.mutex);
                    auto it = bus.conns.find(key);
                    conn = it == bus.conns.end() ? nullptr : it->second;
                }
                if (conn != nullptr) {
                    conn->Send(channel, payload, mode);
                }
            }
        }

        void LoopbackListenerImpl::Close()
        {
            auto &bus = LoopbackBus::Get();
            std::lock_guard<std::mutex> lock(bus.mutex);
            closed = true;
            bus.listeners.erase(address.ToString());
            auto it = bus.backends.find(owner);
            if (it != bus.backends.end()) {
                it->second.listener = nullptr;
            }
        }

    } // namespace

    LoopbackBackend::LoopbackBackend()  = default;
    LoopbackBackend::~LoopbackBackend() { Shutdown(); }

    NetworkBackendCaps LoopbackBackend::MakeDefaultCaps()
    {
        NetworkBackendCaps c;
        c.reliable   = true;
        c.unreliable = true;
        c.ordered    = true;
        c.client     = true;
        c.server     = true;
        c.defaultMode = DeliveryMode::ReliableOrdered;
        c.realSendSequence = true;   // loopback assigns the sequence at send time
        c.threading.supportsCallerPump = true;
        c.threading.supportsHostThread = true;
        c.maxChannels = 8;
        c.maxPayload  = 1200;
        c.mtu         = 1400;
        return c;
    }

    const NetworkBackendCaps &LoopbackBackend::GetCaps() const
    {
        return caps;
    }

    bool LoopbackBackend::Init()
    {
        auto &bus = LoopbackBus::Get();
        std::lock_guard<std::mutex> lock(bus.mutex);
        bus.backends[this];
        return true;
    }

    void LoopbackBackend::Shutdown()
    {
        auto &bus = LoopbackBus::Get();
        std::lock_guard<std::mutex> lock(bus.mutex);

        auto it = bus.backends.find(this);
        if (it != bus.backends.end()) {
            for (auto key : it->second.connKeys) {
                auto connIt = bus.conns.find(key);
                if (connIt == bus.conns.end()) {
                    continue;
                }
                LoopbackConnImpl *conn = connIt->second;
                LoopbackConnImpl *peer = FindPeer(bus, conn);
                if (peer != nullptr) {
                    peer->peerKey = 0;
                    peer->discPending = true;
                }
                conn->peerKey = 0;
                conn->closed = true;
                delete conn;
                bus.conns.erase(connIt);
            }
            if (it->second.listener != nullptr) {
                bus.listeners.erase(it->second.listener->address.ToString());
                delete it->second.listener;
            }
            bus.backends.erase(it);
        }
    }

    INetListener *LoopbackBackend::Listen(const ListenDesc &desc)
    {
        auto &bus = LoopbackBus::Get();
        std::lock_guard<std::mutex> lock(bus.mutex);

        auto *listener = new LoopbackListenerImpl();
        listener->owner   = this;
        listener->address = desc.address;

        auto &entry = bus.backends[this];
        entry.listener = listener;
        bus.listeners[desc.address.ToString()] = this;
        return listener;
    }

    ConnectionId LoopbackBackend::Connect(const ConnectDesc &desc)
    {
        auto &bus = LoopbackBus::Get();
        std::lock_guard<std::mutex> lock(bus.mutex);

        auto listenerIt = bus.listeners.find(desc.address.ToString());
        if (listenerIt == bus.listeners.end()) {
            return INVALID_CONNECTION_ID;
        }
        LoopbackBackend *server = listenerIt->second;

        auto *clientConn = new LoopbackConnImpl();
        auto *serverConn = new LoopbackConnImpl();

        clientConn->key   = bus.nextKey++;
        serverConn->key   = bus.nextKey++;
        clientConn->id    = ConnectionId{bus.nextConnIndex++, 1};
        serverConn->id    = ConnectionId{bus.nextConnIndex++, 1};
        clientConn->owner = this;
        serverConn->owner = server;
        clientConn->peerKey = serverConn->key;
        serverConn->peerKey = clientConn->key;

        bus.conns[clientConn->key] = clientConn;
        bus.conns[serverConn->key] = serverConn;
        bus.backends[this].connKeys.push_back(clientConn->key);
        bus.backends[server].connKeys.push_back(serverConn->key);

        return clientConn->id;
    }

    uint32_t LoopbackBackend::Pump(INetEventSink &sink, uint32_t maxEvents, uint32_t /*waitMs*/)
    {
        struct Delivery {
            enum class Kind { Connected, Message, Disconnected } kind;
            ConnectionId     id;
            ChannelId        channel  = 0;
            MessageSequence  sequence = 0;
            DisconnectReason reason   = DisconnectReason::Unknown;
            std::vector<uint8_t> payload;
        };

        std::vector<Delivery> deliveries;
        {
            auto &bus = LoopbackBus::Get();
            std::lock_guard<std::mutex> lock(bus.mutex);

            auto entryIt = bus.backends.find(this);
            if (entryIt == bus.backends.end()) {
                return 0;
            }
            const auto keys = entryIt->second.connKeys;

            for (auto key : keys) {
                if (deliveries.size() >= maxEvents) {
                    break;
                }
                auto connIt = bus.conns.find(key);
                if (connIt == bus.conns.end()) {
                    continue;
                }
                LoopbackConnImpl *conn = connIt->second;

                if (!conn->connectedEmitted && !conn->closed) {
                    conn->connectedEmitted = true;
                    Delivery d;
                    d.kind = Delivery::Kind::Connected;
                    d.id   = conn->id;
                    deliveries.push_back(std::move(d));
                }

                while (!conn->inbox.empty() && deliveries.size() < maxEvents) {
                    Delivery d;
                    d.kind     = Delivery::Kind::Message;
                    d.id       = conn->id;
                    d.channel  = conn->inbox.front().channel;
                    d.sequence = conn->inbox.front().sequence;
                    d.payload  = std::move(conn->inbox.front().payload);
                    conn->inbox.pop_front();
                    deliveries.push_back(std::move(d));
                }

                if (conn->discPending && !conn->discEmitted) {
                    conn->discEmitted = true;
                    Delivery d;
                    d.kind   = Delivery::Kind::Disconnected;
                    d.id     = conn->id;
                    d.reason = conn->discReason;
                    deliveries.push_back(std::move(d));
                }
            }
        }

        for (auto &delivery : deliveries) {
            switch (delivery.kind) {
            case Delivery::Kind::Connected:
                sink.OnConnected(delivery.id);
                break;
            case Delivery::Kind::Message:
                sink.OnMessage(delivery.id, delivery.channel, delivery.sequence, delivery.payload);
                break;
            case Delivery::Kind::Disconnected:
                sink.OnDisconnected(delivery.id, delivery.reason);
                break;
            }
        }
        return static_cast<uint32_t>(deliveries.size());
    }

    INetConnection *LoopbackBackend::GetConnection(ConnectionId id)
    {
        auto &bus = LoopbackBus::Get();
        std::lock_guard<std::mutex> lock(bus.mutex);
        auto it = bus.conns.find(id.index);
        return it == bus.conns.end() ? nullptr : it->second;
    }

    void LoopbackBackend::ForceDisconnect(ConnectionId id, DisconnectReason reason)
    {
        if (auto *conn = GetConnection(id)) {
            conn->Close(reason);
        }
    }

    void LoopbackBackend::SetRtt(ConnectionId id, uint32_t ms)
    {
        if (auto *conn = static_cast<LoopbackConnImpl *>(GetConnection(id))) {
            conn->rttMs = ms;
        }
    }

    bool LoopbackBackend::ShouldDropNextSend()
    {
        if (dropEveryN == 0) {
            return false;
        }
        ++sendCounter;
        if (sendCounter % dropEveryN == 0) {
            ++dropped;
            return true;
        }
        return false;
    }

} // namespace sky::net::test

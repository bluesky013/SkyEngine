//
// Created on 2026/09/25.
//

#include <network/enet/EnetBackend.h>

#include <enet.h>

#include <cstring>
#include <unordered_map>
#include <vector>

namespace sky::net {

    namespace {

        constexpr uint32_t CHANNEL_COUNT = 4;

        // ENet reliability is per packet; channels keep ordered streams separate. Mapping matches the
        // engine delivery modes.
        struct EnetSendMode {
            enet_uint8  channel = 0;
            enet_uint32 flags   = 0;
        };

        EnetSendMode MapMode(DeliveryMode mode)
        {
            switch (mode) {
            case DeliveryMode::ReliableOrdered:    return {0, ENET_PACKET_FLAG_RELIABLE};
            case DeliveryMode::ReliableUnordered:  return {1, ENET_PACKET_FLAG_RELIABLE | ENET_PACKET_FLAG_UNSEQUENCED};
            case DeliveryMode::UnreliableSequenced:return {2, 0};
            case DeliveryMode::Unreliable:         return {3, ENET_PACKET_FLAG_UNSEQUENCED};
            }
            return {0, ENET_PACKET_FLAG_RELIABLE};
        }

        DisconnectReason MapReason(enet_uint8 data)
        {
            return data == 0 ? DisconnectReason::RemoteClose : DisconnectReason::ProtocolError;
        }

        bool ResolveAddress(const NetworkAddress &address, ENetAddress &out)
        {
            out.port = address.port;
            // set_host accepts both IP literals and hostnames and is available regardless of the
            // address-mapping feature selection.
            return enet_address_set_host(&out, address.host.c_str()) == 0;
        }

    } // namespace

    struct EnetBackend::Impl {
        struct Record;
        struct Listener;

        ENetHost *host = nullptr;
        bool      server = false;
        uint64_t  nextIndex = 1;
        Listener *listener = nullptr;
        std::unordered_map<uint64_t, Record *> byIndex;
        std::vector<Record *> records;

        NetResult SendOnRecord(Record *record, ChannelId channel, std::span<const uint8_t> payload, DeliveryMode mode);
        void      CloseRecord(Record *record, DisconnectReason reason);
        void      CleanupRecord(Record *record);
    };

    struct EnetBackend::Impl::Record : public INetConnection {
        Impl        *impl = nullptr;
        ENetPeer    *peer = nullptr;
        ConnectionId id;
        uint32_t     recvSeq = 0;
        uint32_t     rttMs = 0;
        bool         connectedEmitted = false;

        ConnectionId GetId() const override { return id; }

        NetResult Send(ChannelId channel, std::span<const uint8_t> payload, DeliveryMode mode) override
        {
            return impl->SendOnRecord(this, channel, payload, mode);
        }

        void Close(DisconnectReason reason) override { impl->CloseRecord(this, reason); }

        uint32_t GetRttMs() const override { return rttMs; }
    };

    struct EnetBackend::Impl::Listener : public INetListener {
        Impl        *impl = nullptr;
        NetworkAddress address;
        bool         closed = false;

        void Broadcast(ChannelId channel, std::span<const uint8_t> payload, DeliveryMode mode) override
        {
            for (auto *record : impl->records) {
                impl->SendOnRecord(record, channel, payload, mode);
            }
        }

        NetworkAddress GetAddress() const override { return address; }

        void Close() override { closed = true; }
    };

    NetResult EnetBackend::Impl::SendOnRecord(Record *record, ChannelId channel, std::span<const uint8_t> payload,
                                              DeliveryMode mode)
    {
        if (record == nullptr || record->peer == nullptr || host == nullptr) {
            return NetResult::NotConnected;
        }
        const EnetSendMode mapped = MapMode(mode);
        if (channel >= CHANNEL_COUNT) {
            return NetResult::InvalidArgument;
        }
        ENetPacket *packet = enet_packet_create(payload.data(), payload.size(), mapped.flags);
        if (packet == nullptr) {
            return NetResult::QueueFull;
        }
        if (enet_peer_send(record->peer, channel, packet) != 0) {
            enet_packet_destroy(packet);
            return NetResult::NotConnected;
        }
        enet_host_flush(host);
        return NetResult::Ok;
    }

    void EnetBackend::Impl::CloseRecord(Record *record, DisconnectReason /*reason*/)
    {
        if (record != nullptr && record->peer != nullptr) {
            enet_peer_disconnect(record->peer, 0);
            if (host != nullptr) {
                enet_host_flush(host);
            }
        }
    }

    void EnetBackend::Impl::CleanupRecord(Record *record)
    {
        if (record == nullptr) {
            return;
        }
        if (record->peer != nullptr) {
            record->peer->data = nullptr;
        }
        byIndex.erase(record->id.index);
        for (auto it = records.begin(); it != records.end(); ++it) {
            if (*it == record) {
                records.erase(it);
                break;
            }
        }
        delete record;
    }

    EnetBackend::EnetBackend() : impl(std::make_unique<Impl>()) {}
    EnetBackend::~EnetBackend() { Shutdown(); }

    const NetworkBackendCaps &EnetBackend::GetCaps() const
    {
        static const NetworkBackendCaps caps = []() {
            NetworkBackendCaps c;
            c.reliable   = true;
            c.unreliable = true;
            c.ordered    = true;
            c.encryption = false;
            c.client     = true;
            c.server     = true;
            c.peerToPeer = false;
            c.defaultMode = DeliveryMode::ReliableOrdered;
            c.threading.supportsCallerPump = true;
            c.threading.supportsHostThread = true;
            c.threading.backendOwnsThreads = false;
            c.threading.threadSafeSend     = false;
            c.threading.wakeupSupport      = false;
            c.maxChannels = CHANNEL_COUNT;
            c.maxPayload  = 1024;
            c.mtu         = 1400;
            return c;
        }();
        return caps;
    }

    bool EnetBackend::Init()
    {
        return enet_initialize() == 0;
    }

    void EnetBackend::Shutdown()
    {
        if (!impl) {
            return;
        }
        for (auto *record : impl->records) {
            delete record;
        }
        impl->records.clear();
        impl->byIndex.clear();
        delete impl->listener;
        impl->listener = nullptr;
        if (impl->host != nullptr) {
            enet_host_destroy(impl->host);
            impl->host = nullptr;
        }
        enet_deinitialize();
    }

    INetListener *EnetBackend::Listen(const ListenDesc &desc)
    {
        ENetAddress address;
        std::memset(&address, 0, sizeof(address));
        if (!ResolveAddress(desc.address, address)) {
            return nullptr;
        }
        ENetHost *host = enet_host_create(&address, 32, CHANNEL_COUNT, 0, 0);
        if (host == nullptr) {
            return nullptr;
        }
        impl->host   = host;
        impl->server = true;
        impl->listener = new Impl::Listener();
        impl->listener->impl = impl.get();
        impl->listener->address = desc.address;
        return impl->listener;
    }

    ConnectionId EnetBackend::Connect(const ConnectDesc &desc)
    {
        if (impl->host == nullptr) {
            impl->host = enet_host_create(nullptr, 1, CHANNEL_COUNT, 0, 0);
            if (impl->host == nullptr) {
                return INVALID_CONNECTION_ID;
            }
            impl->server = false;
        }
        ENetAddress address;
        std::memset(&address, 0, sizeof(address));
        if (!ResolveAddress(desc.address, address)) {
            return INVALID_CONNECTION_ID;
        }
        ENetPeer *peer = enet_host_connect(impl->host, &address, CHANNEL_COUNT, 0);
        if (peer == nullptr) {
            return INVALID_CONNECTION_ID;
        }

        auto *record = new Impl::Record();
        record->impl = impl.get();
        record->peer = peer;
        record->id   = ConnectionId{impl->nextIndex++, 1};
        peer->data   = record;
        impl->byIndex[record->id.index] = record;
        impl->records.push_back(record);
        return record->id;
    }

    uint32_t EnetBackend::Pump(INetEventSink &sink, uint32_t maxEvents, uint32_t waitMs)
    {
        if (impl->host == nullptr) {
            return 0;
        }

        uint32_t count = 0;
        uint32_t wait  = waitMs;
        while (count < maxEvents) {
            ENetEvent event;
            const int result = enet_host_service(impl->host, &event, wait);
            wait = 0;
            if (result <= 0) {
                break;
            }

            switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT: {
                auto *record = static_cast<Impl::Record *>(event.peer->data);
                if (record == nullptr) {
                    record = new Impl::Record();
                    record->impl = impl.get();
                    record->peer = event.peer;
                    record->id   = ConnectionId{impl->nextIndex++, 1};
                    event.peer->data = record;
                    impl->byIndex[record->id.index] = record;
                    impl->records.push_back(record);
                }
                if (!record->connectedEmitted) {
                    record->connectedEmitted = true;
                    sink.OnConnected(record->id);
                    ++count;
                }
                break;
            }
            case ENET_EVENT_TYPE_RECEIVE: {
                auto *record = static_cast<Impl::Record *>(event.peer->data);
                if (record != nullptr) {
                    sink.OnMessage(record->id, event.channelID, record->recvSeq++,
                                   std::span<const uint8_t>(event.packet->data, event.packet->dataLength));
                    ++count;
                }
                enet_packet_destroy(event.packet);
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT: {
                auto *record = static_cast<Impl::Record *>(event.peer->data);
                if (record != nullptr) {
                    sink.OnDisconnected(record->id, MapReason(event.data));
                    ++count;
                    impl->CleanupRecord(record);
                }
                break;
            }
            default:
                break;
            }
        }
        return count;
    }

    INetConnection *EnetBackend::GetConnection(ConnectionId id)
    {
        auto it = impl->byIndex.find(id.index);
        return it == impl->byIndex.end() ? nullptr : it->second;
    }

} // namespace sky::net

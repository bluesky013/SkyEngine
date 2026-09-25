//
// Created on 2026/09/25.
//

#pragma once

#include <network/ConnectionId.h>
#include <network/INetBackend.h>
#include <network/NetworkAddress.h>
#include <network/NetworkCaps.h>
#include <network/NetworkHostConfig.h>
#include <network/NetworkLane.h>
#include <network/NetworkSession.h>
#include <network/NetworkStats.h>
#include <network/NetworkTypes.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <span>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sky::net {

    // Reserved channel for host control frames (hello/hello-ack/redirect/heartbeat). User payloads must
    // not use it.
    inline constexpr ChannelId CONTROL_CHANNEL = 0xFF;

    enum class NetControlType : uint8_t {
        Hello = 1,
        HelloAck = 2,
        Redirect = 3,
        Heartbeat = 4
    };

    // Host-agnostic network host: composes one backend per role, drains events on the caller thread, and
    // owns the threading and timing policy. It has no World/ECS/render dependency.
    class NetworkHost final : private INetEventSink {
    public:
        using ConnectHandler    = std::function<void(ConnectionId)>;
        using DisconnectHandler = std::function<void(ConnectionId, DisconnectReason)>;
        using MessageHandler    = std::function<void(ConnectionId, ChannelId, MessageSequence, std::span<const uint8_t>)>;
        using ErrorHandler      = std::function<void(ConnectionId, NetResult)>;
        using RedirectHandler   = std::function<void(const NetworkAddress &, SessionId, const ResumeToken &)>;
        using SessionHandler    = std::function<void(ConnectionId, SessionId)>;

        explicit NetworkHost(const NetworkHostConfig &config = {});
        ~NetworkHost() override;

        // Backends are owned elsewhere (the registry) and attached for a role. Several roles may be
        // active concurrently.
        bool         AttachBackend(NetworkRole role, INetBackend *backend);
        void         DetachBackend(NetworkRole role);
        INetBackend *GetBackend(NetworkRole role) const;

        void            SetThreadingModel(NetworkThreading model);
        NetworkThreading GetThreadingModel() const { return threading; }

        void     SetClock(std::function<uint64_t()> clock);
        uint64_t Now() const;

        // Server-side token authority. Clients only need the codec to decode HelloAck.
        void SetTokenCodec(const ResumeTokenCodec &codec);
        void SetServer(bool isServer);

        void SetConnectHandler(ConnectHandler handler);
        void SetDisconnectHandler(DisconnectHandler handler);
        void SetMessageHandler(MessageHandler handler);
        void SetErrorHandler(ErrorHandler handler);
        void SetRedirectHandler(RedirectHandler handler);
        void SetSessionHandler(SessionHandler handler);

        bool Start();
        void Update();
        void Stop();

        void               DrainToClosed();
        NetworkLifecycle   GetLifecycleState() const { return lifecycle; }

        INetListener *Listen(const NetworkAddress &address);
        ConnectionId  Connect(const NetworkAddress &address, const ResumeToken *resume = nullptr);
        NetResult    Send(ConnectionId id, ChannelId channel, std::span<const uint8_t> payload,
                          DeliveryMode mode = DeliveryMode::ReliableOrdered);
        void         Disconnect(ConnectionId id, DisconnectReason reason = DisconnectReason::LocalClose);
        NetResult    RedirectTo(ConnectionId id, const NetworkAddress &target);

        SessionId    GetSession(ConnectionId id) const;
        ConnectionId GetConnection(SessionId session) const;
        bool         BindSession(ConnectionId id, SessionId session);

        // Last resume token assigned by a server (client side), useful for reconnect.
        ResumeToken  GetResumeToken() const { return lastResumeToken; }

        // Lane partitioning (fixed at startup).
        uint32_t GetLaneIndex(ConnectionId id) const;
        uint32_t GetLaneCount() const { return static_cast<uint32_t>(lanes.size()); }

        NetworkHostStats GetStats() const;

    private:
        struct QueuedEvent {
            NetEventType     type       = NetEventType::Error;
            ConnectionId     connection = INVALID_CONNECTION_ID;
            ChannelId        channel    = 0;
            MessageSequence  sequence   = 0;
            DisconnectReason reason     = DisconnectReason::Unknown;
            NetResult        error      = NetResult::Ok;
            std::vector<uint8_t> payload;
        };

        // INetEventSink
        void OnConnected(ConnectionId id) override;
        void OnDisconnected(ConnectionId id, DisconnectReason reason) override;
        void OnMessage(ConnectionId id, ChannelId channel, MessageSequence sequence,
                       std::span<const uint8_t> payload) override;
        void OnError(ConnectionId id, NetResult error) override;

        void PushEvent(QueuedEvent &&event);
        void PumpAll(uint32_t waitMs);
        void DrainEvents();
        void HandleOverflow();
        void RunTimers();
        void ProcessReconnect();

        void HandleControl(ConnectionId id, std::span<const uint8_t> payload);
        void SendControl(ConnectionId id, NetControlType type, std::span<const uint8_t> body);

        INetBackend *BackendFor(ConnectionId id) const;
        NetworkLane *LaneFor(ConnectionId id);
        uint32_t     LaneIndexFor(ConnectionId id) const;

        NetworkHostConfig config;
        NetworkThreading  threading = NetworkThreading::CallerPump;
        bool              server    = false;
        NetworkLifecycle  lifecycle = NetworkLifecycle::Accepting;

        std::function<uint64_t()> clock;
        ResumeTokenCodec          tokenCodec{0};
        uint32_t                  nonceCounter = 0;

        std::array<INetBackend *, static_cast<size_t>(NetworkRole::Count)> backends{};
        INetBackend *pumpingBackend = nullptr;

        std::vector<NetworkLane> lanes;

        ConnectHandler    connectHandler;
        DisconnectHandler disconnectHandler;
        MessageHandler    messageHandler;
        ErrorHandler      errorHandler;
        RedirectHandler   redirectHandler;
        SessionHandler    sessionHandler;

        mutable std::mutex    queueMutex;
        std::deque<QueuedEvent> eventQueue;
        uint32_t              queueCapacity = 1;
        ConnectionId          overflowConnection = INVALID_CONNECTION_ID;

        std::thread       ioThread;
        std::atomic<bool> running{false};

        std::unordered_map<ConnectionId, INetBackend *, ConnectionIdHash> connectionBackends;
        std::unordered_set<ConnectionId, ConnectionIdHash>                connected;
        std::unordered_map<ConnectionId, uint64_t, ConnectionIdHash>      lastActivityMs;
        std::unordered_map<ConnectionId, uint64_t, ConnectionIdHash>      lastHeartbeatMs;
        std::unordered_map<ConnectionId, SessionId, ConnectionIdHash>     connectionSessions;
        std::unordered_map<uint64_t, std::vector<ConnectionId>>           sessionConnections;
        std::unordered_map<ConnectionId, ResumeToken, ConnectionIdHash>   pendingTokens;
        uint64_t                                                          sessionCounter = 0;

        NetworkAddress lastConnectAddress;
        ResumeToken    lastResumeToken;
        bool           hasConnectTarget      = false;
        uint64_t       nextReconnectAtMs     = 0;
        uint32_t       reconnectBackoffMs    = 0;

        uint64_t bytesIn         = 0;
        uint64_t bytesOut        = 0;
        uint64_t messagesDropped = 0;
        uint64_t reconnectAttempts = 0;
    };

} // namespace sky::net

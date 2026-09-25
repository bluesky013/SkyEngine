//
// Created on 2026/09/25.
//

#include <network/NetworkHost.h>

#include <core/logger/Logger.h>

#include <algorithm>
#include <chrono>

static const char *TAG = "NetworkHost";

namespace sky::net {

    namespace {

        uint64_t DefaultNowMs()
        {
            using namespace std::chrono;
            return static_cast<uint64_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
        }

        void AppendU16(std::vector<uint8_t> &out, uint16_t value)
        {
            out.push_back(static_cast<uint8_t>(value & 0xFFu));
            out.push_back(static_cast<uint8_t>((value >> 8u) & 0xFFu));
        }

        void AppendU64(std::vector<uint8_t> &out, uint64_t value)
        {
            for (uint32_t i = 0; i < 8; ++i) {
                out.push_back(static_cast<uint8_t>((value >> (i * 8u)) & 0xFFu));
            }
        }

        bool ReadU16(const uint8_t *data, uint32_t size, uint32_t &offset, uint16_t &out)
        {
            if (offset + 2 > size) {
                return false;
            }
            out = static_cast<uint16_t>(data[offset]) | (static_cast<uint16_t>(data[offset + 1]) << 8u);
            offset += 2;
            return true;
        }

        bool ReadU64(const uint8_t *data, uint32_t size, uint32_t &offset, uint64_t &out)
        {
            if (offset + 8 > size) {
                return false;
            }
            out = 0;
            for (uint32_t i = 0; i < 8; ++i) {
                out |= static_cast<uint64_t>(data[offset + i]) << (i * 8u);
            }
            offset += 8;
            return true;
        }

    } // namespace

    NetworkHost::NetworkHost(const NetworkHostConfig &cfg) : config(cfg), clock(&DefaultNowMs)
    {
        queueCapacity = config.eventQueueCapacity == 0 ? 1 : config.eventQueueCapacity;
        reconnectBackoffMs = config.reconnectInitialBackoffMs;

        const uint32_t laneCount = config.laneCount == 0 ? 1 : config.laneCount;
        lanes.resize(laneCount);
        for (uint32_t i = 0; i < laneCount; ++i) {
            lanes[i].id = i;
        }
    }

    NetworkHost::~NetworkHost()
    {
        Stop();
    }

    bool NetworkHost::AttachBackend(NetworkRole role, INetBackend *backend)
    {
        if (role >= NetworkRole::Count || backend == nullptr) {
            return false;
        }
        backends[static_cast<size_t>(role)] = backend;
        return true;
    }

    void NetworkHost::DetachBackend(NetworkRole role)
    {
        if (role < NetworkRole::Count) {
            backends[static_cast<size_t>(role)] = nullptr;
        }
    }

    INetBackend *NetworkHost::GetBackend(NetworkRole role) const
    {
        if (role >= NetworkRole::Count) {
            return nullptr;
        }
        return backends[static_cast<size_t>(role)];
    }

    void NetworkHost::SetThreadingModel(NetworkThreading model) { threading = model; }

    void NetworkHost::SetClock(std::function<uint64_t()> fn)
    {
        if (fn) {
            clock = std::move(fn);
        }
    }

    uint64_t NetworkHost::Now() const { return clock ? clock() : DefaultNowMs(); }

    void NetworkHost::SetTokenCodec(const ResumeTokenCodec &codec) { tokenCodec = codec; }
    void NetworkHost::SetServer(bool isServer) { server = isServer; }

    void NetworkHost::SetConnectHandler(ConnectHandler handler) { connectHandler = std::move(handler); }
    void NetworkHost::SetDisconnectHandler(DisconnectHandler handler) { disconnectHandler = std::move(handler); }
    void NetworkHost::SetMessageHandler(MessageHandler handler) { messageHandler = std::move(handler); }
    void NetworkHost::SetErrorHandler(ErrorHandler handler) { errorHandler = std::move(handler); }
    void NetworkHost::SetRedirectHandler(RedirectHandler handler) { redirectHandler = std::move(handler); }
    void NetworkHost::SetSessionHandler(SessionHandler handler) { sessionHandler = std::move(handler); }

    bool NetworkHost::Start()
    {
        if (threading == NetworkThreading::OwnedThread && !running.exchange(true)) {
            ioThread = std::thread([this]() {
                while (running.load()) {
                    uint32_t pumped = 0;
                    for (auto *backend : backends) {
                        if (backend != nullptr) {
                            pumpingBackend = backend;
                            pumped += backend->Pump(*this, config.pumpEventBudget, config.ioWaitMs);
                        }
                    }
                    pumpingBackend = nullptr;
                    if (pumped == 0) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                }
            });
        }
        return true;
    }

    void NetworkHost::Stop()
    {
        running = false;
        if (ioThread.joinable()) {
            ioThread.join();
        }
        if (lifecycle != NetworkLifecycle::Closed) {
            lifecycle = NetworkLifecycle::Closed;
        }
    }

    void NetworkHost::DrainToClosed()
    {
        if (lifecycle == NetworkLifecycle::Accepting) {
            lifecycle = NetworkLifecycle::Draining;
        }
    }

    void NetworkHost::Update()
    {
        if (threading == NetworkThreading::CallerPump) {
            PumpAll(0);
        }
        DrainEvents();
        HandleOverflow();
        RunTimers();
        ProcessReconnect();
    }

    void NetworkHost::PumpAll(uint32_t waitMs)
    {
        for (auto *backend : backends) {
            if (backend != nullptr) {
                pumpingBackend = backend;
                backend->Pump(*this, config.pumpEventBudget, waitMs);
            }
        }
        pumpingBackend = nullptr;
    }

    INetListener *NetworkHost::Listen(const NetworkAddress &address)
    {
        auto *backend = backends[static_cast<size_t>(NetworkRole::Server)];
        if (backend == nullptr) {
            return nullptr;
        }
        return backend->Listen(ListenDesc{address, 0});
    }

    ConnectionId NetworkHost::Connect(const NetworkAddress &address, const ResumeToken *resume)
    {
        auto *backend = backends[static_cast<size_t>(NetworkRole::Client)];
        if (backend == nullptr) {
            return INVALID_CONNECTION_ID;
        }
        lastConnectAddress = address;
        hasConnectTarget    = true;
        lastResumeToken     = resume != nullptr ? *resume : ResumeToken{};

        ConnectionId id = backend->Connect(ConnectDesc{address, 0});
        if (IsValid(id) && resume != nullptr) {
            pendingTokens[id] = *resume;
        }
        return id;
    }

    NetResult NetworkHost::Send(ConnectionId id, ChannelId channel, std::span<const uint8_t> payload, DeliveryMode mode)
    {
        auto *backend = BackendFor(id);
        if (backend == nullptr) {
            return NetResult::NotConnected;
        }
        auto *connection = backend->GetConnection(id);
        if (connection == nullptr) {
            return NetResult::NotConnected;
        }
        const auto &caps = backend->GetCaps();
        if (!caps.Supports(mode)) {
            return NetResult::UnsupportedDeliveryMode;
        }
        if (payload.size() > caps.maxPayload) {
            return NetResult::PayloadTooLarge;
        }
        NetResult result = connection->Send(channel, payload, mode);
        if (result == NetResult::Ok) {
            bytesOut += payload.size();
            if (auto *lane = LaneFor(id)) {
                lane->bytesOut += payload.size();
            }
        }
        return result;
    }

    void NetworkHost::Disconnect(ConnectionId id, DisconnectReason reason)
    {
        if (auto *backend = BackendFor(id)) {
            if (auto *connection = backend->GetConnection(id)) {
                connection->Close(reason);
            }
        }
    }

    NetResult NetworkHost::RedirectTo(ConnectionId id, const NetworkAddress &target)
    {
        SessionId session = GetSession(id);
        if (!IsValid(session)) {
            return NetResult::NotFound;
        }
        ResumeToken token = tokenCodec.Issue(session, Now(), config.resumeTokenTtlMs, ++nonceCounter);

        const std::string addressText = target.ToString();
        if (addressText.size() > 0xFFFFu) {
            return NetResult::InvalidArgument;
        }

        std::vector<uint8_t> body;
        body.reserve(2 + addressText.size() + ResumeToken::SERIALIZED_SIZE);
        AppendU16(body, static_cast<uint16_t>(addressText.size()));
        body.insert(body.end(), addressText.begin(), addressText.end());
        uint8_t tokenBytes[ResumeToken::SERIALIZED_SIZE] = {};
        token.Serialize(tokenBytes, sizeof(tokenBytes));
        body.insert(body.end(), tokenBytes, tokenBytes + ResumeToken::SERIALIZED_SIZE);

        SendControl(id, NetControlType::Redirect, body);
        return NetResult::Ok;
    }

    SessionId NetworkHost::GetSession(ConnectionId id) const
    {
        auto it = connectionSessions.find(id);
        return it == connectionSessions.end() ? INVALID_SESSION_ID : it->second;
    }

    ConnectionId NetworkHost::GetConnection(SessionId session) const
    {
        auto it = sessionConnections.find(session.value);
        if (it == sessionConnections.end() || it->second.empty()) {
            return INVALID_CONNECTION_ID;
        }
        return it->second.front();
    }

    bool NetworkHost::BindSession(ConnectionId id, SessionId session)
    {
        if (!IsValid(id)) {
            return false;
        }
        if (!IsValid(session)) {
            session.value = ++sessionCounter;
        }
        connectionSessions[id] = session;
        auto &connections = sessionConnections[session.value];
        if (std::find(connections.begin(), connections.end(), id) == connections.end()) {
            connections.push_back(id);
        }
        return true;
    }

    NetworkHostStats NetworkHost::GetStats() const
    {
        NetworkHostStats stats;
        stats.activeConnections = static_cast<uint32_t>(connected.size());
        stats.laneCount         = static_cast<uint32_t>(lanes.size());
        stats.bytesIn           = bytesIn;
        stats.bytesOut          = bytesOut;
        stats.messagesDropped   = messagesDropped;
        stats.reconnectAttempts = reconnectAttempts;
        for (auto id : connected) {
            if (auto *backend = BackendFor(id)) {
                if (auto *connection = backend->GetConnection(id)) {
                    stats.rttMs = std::max(stats.rttMs, connection->GetRttMs());
                }
            }
        }
        for (size_t i = 0; i < lanes.size() && i < NETWORK_MAX_LANES; ++i) {
            stats.lanes[i].connections = static_cast<uint32_t>(lanes[i].connections.size());
            stats.lanes[i].bytesIn     = lanes[i].bytesIn;
            stats.lanes[i].bytesOut    = lanes[i].bytesOut;
        }
        return stats;
    }

    INetBackend *NetworkHost::BackendFor(ConnectionId id) const
    {
        auto it = connectionBackends.find(id);
        return it == connectionBackends.end() ? nullptr : it->second;
    }

    uint32_t NetworkHost::LaneIndexFor(ConnectionId id) const
    {
        if (lanes.empty()) {
            return 0;
        }
        return static_cast<uint32_t>(ConnectionIdHash{}(id) % lanes.size());
    }

    NetworkLane *NetworkHost::LaneFor(ConnectionId id)
    {
        if (lanes.empty()) {
            return nullptr;
        }
        return &lanes[LaneIndexFor(id)];
    }

    uint32_t NetworkHost::GetLaneIndex(ConnectionId id) const
    {
        return LaneIndexFor(id);
    }

    void NetworkHost::PushEvent(QueuedEvent &&event)
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        if (eventQueue.size() >= queueCapacity) {
            messagesDropped++;
            overflowConnection = event.connection;
            return;
        }
        eventQueue.emplace_back(std::move(event));
    }

    void NetworkHost::DrainEvents()
    {
        std::deque<QueuedEvent> local;
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            local.swap(eventQueue);
        }

        for (auto &event : local) {
            switch (event.type) {
            case NetEventType::Connected:
                if (connectHandler) {
                    connectHandler(event.connection);
                }
                break;
            case NetEventType::Disconnected:
                if (disconnectHandler) {
                    disconnectHandler(event.connection, event.reason);
                }
                break;
            case NetEventType::Message:
                if (event.channel == CONTROL_CHANNEL) {
                    HandleControl(event.connection, event.payload);
                } else if (messageHandler) {
                    messageHandler(event.connection, event.channel, event.sequence, event.payload);
                }
                break;
            case NetEventType::Error:
                if (errorHandler) {
                    errorHandler(event.connection, event.error);
                }
                break;
            }
        }
    }

    void NetworkHost::HandleOverflow()
    {
        if (!IsValid(overflowConnection)) {
            return;
        }
        ConnectionId id = overflowConnection;
        overflowConnection = INVALID_CONNECTION_ID;
        if (auto *backend = BackendFor(id)) {
            if (auto *connection = backend->GetConnection(id)) {
                connection->Close(DisconnectReason::Backpressure);
            }
        }
        if (disconnectHandler) {
            disconnectHandler(id, DisconnectReason::Backpressure);
        }
    }

    void NetworkHost::RunTimers()
    {
        const uint64_t now = Now();

        for (auto id : connected) {
            if (config.heartbeatIntervalMs > 0) {
                auto &lastBeat = lastHeartbeatMs[id];
                if (now - lastBeat >= config.heartbeatIntervalMs) {
                    lastBeat = now;
                    SendControl(id, NetControlType::Heartbeat, {});
                }
            }
        }

        std::vector<ConnectionId> timedOut;
        for (auto id : connected) {
            auto it = lastActivityMs.find(id);
            if (config.timeoutMs > 0 && it != lastActivityMs.end() && now - it->second > config.timeoutMs) {
                timedOut.push_back(id);
            }
        }
        for (auto id : timedOut) {
            Disconnect(id, DisconnectReason::Timeout);
        }
    }

    void NetworkHost::ProcessReconnect()
    {
        if (server || !config.autoReconnect || !hasConnectTarget) {
            return;
        }
        if (!pendingTokens.empty() || !connected.empty()) {
            return;
        }
        const uint64_t now = Now();
        if (now < nextReconnectAtMs) {
            return;
        }
        auto *backend = backends[static_cast<size_t>(NetworkRole::Client)];
        if (backend == nullptr) {
            return;
        }
        reconnectAttempts++;
        ConnectionId id = backend->Connect(ConnectDesc{lastConnectAddress, 0});
        if (IsValid(id)) {
            pendingTokens[id] = lastResumeToken;
            nextReconnectAtMs  = now + reconnectBackoffMs;
            reconnectBackoffMs = std::min(reconnectBackoffMs * 2, config.reconnectMaxBackoffMs);
        } else {
            nextReconnectAtMs = now + reconnectBackoffMs;
            reconnectBackoffMs = std::min(reconnectBackoffMs * 2, config.reconnectMaxBackoffMs);
        }
    }

    void NetworkHost::SendControl(ConnectionId id, NetControlType type, std::span<const uint8_t> body)
    {
        auto *backend = BackendFor(id);
        if (backend == nullptr) {
            return;
        }
        auto *connection = backend->GetConnection(id);
        if (connection == nullptr) {
            return;
        }
        std::vector<uint8_t> frame;
        frame.reserve(1 + body.size());
        frame.push_back(static_cast<uint8_t>(type));
        frame.insert(frame.end(), body.begin(), body.end());
        if (connection->Send(CONTROL_CHANNEL, frame, DeliveryMode::ReliableOrdered) == NetResult::Ok) {
            bytesOut += frame.size();
            if (auto *lane = LaneFor(id)) {
                lane->bytesOut += frame.size();
            }
        }
    }

    void NetworkHost::HandleControl(ConnectionId id, std::span<const uint8_t> payload)
    {
        if (payload.empty()) {
            return;
        }
        const auto type = static_cast<NetControlType>(payload[0]);
        const uint8_t *data = payload.data() + 1;
        const uint32_t size = static_cast<uint32_t>(payload.size() - 1);

        switch (type) {
        case NetControlType::Heartbeat:
            break;
        case NetControlType::Hello: {
            if (!server) {
                break;
            }
            ResumeToken token;
            ResumeToken::Deserialize(data, size, token);
            const bool valid = !token.IsZero() && tokenCodec.Verify(token, Now());
            SessionId session = valid ? SessionId{token.session} : INVALID_SESSION_ID;
            BindSession(id, session);
            SessionId bound = GetSession(id);

            ResumeToken assigned = tokenCodec.Issue(bound, Now(), config.resumeTokenTtlMs, ++nonceCounter);
            std::vector<uint8_t> body;
            AppendU64(body, bound.value);
            uint8_t tokenBytes[ResumeToken::SERIALIZED_SIZE] = {};
            assigned.Serialize(tokenBytes, sizeof(tokenBytes));
            body.insert(body.end(), tokenBytes, tokenBytes + ResumeToken::SERIALIZED_SIZE);
            SendControl(id, NetControlType::HelloAck, body);
            break;
        }
        case NetControlType::HelloAck: {
            uint32_t offset = 0;
            uint64_t sessionValue = 0;
            if (!ReadU64(data, size, offset, sessionValue)) {
                break;
            }
            ResumeToken token;
            if (!ResumeToken::Deserialize(data + offset, size - offset, token)) {
                break;
            }
            SessionId session{sessionValue};
            BindSession(id, session);
            lastResumeToken = token;
            if (sessionHandler) {
                sessionHandler(id, session);
            }
            break;
        }
        case NetControlType::Redirect: {
            uint32_t offset = 0;
            uint16_t length = 0;
            if (!ReadU16(data, size, offset, length) || offset + length > size) {
                break;
            }
            std::string addressText(reinterpret_cast<const char *>(data + offset), length);
            offset += length;
            ResumeToken token;
            ResumeToken::Deserialize(data + offset, size - offset, token);
            NetworkAddress address = NetworkAddress::Parse(addressText);
            if (redirectHandler) {
                redirectHandler(address, SessionId{token.session}, token);
            }
            Disconnect(id, DisconnectReason::Redirect);
            break;
        }
        }
    }

    void NetworkHost::OnConnected(ConnectionId id)
    {
        connectionBackends[id] = pumpingBackend;
        connected.insert(id);
        lastActivityMs[id]  = Now();
        lastHeartbeatMs[id] = Now();

        if (server && lifecycle != NetworkLifecycle::Accepting) {
            // Reject new incoming connections while draining/closing.
            connected.erase(id);
            connectionBackends.erase(id);
            if (auto *backend = pumpingBackend) {
                if (auto *connection = backend->GetConnection(id)) {
                    connection->Close(DisconnectReason::ServerShutdown);
                }
            }
            QueuedEvent event;
            event.type       = NetEventType::Disconnected;
            event.connection = id;
            event.reason     = DisconnectReason::ServerShutdown;
            PushEvent(std::move(event));
            return;
        }

        if (!server) {
            // Client announces itself (with a resume token when reconnecting).
            auto it = pendingTokens.find(id);
            ResumeToken token = it != pendingTokens.end() ? it->second : lastResumeToken;
            std::vector<uint8_t> body;
            if (!token.IsZero()) {
                uint8_t tokenBytes[ResumeToken::SERIALIZED_SIZE] = {};
                token.Serialize(tokenBytes, sizeof(tokenBytes));
                body.insert(body.end(), tokenBytes, tokenBytes + ResumeToken::SERIALIZED_SIZE);
            }
            SendControl(id, NetControlType::Hello, body);
        }

        if (auto *lane = LaneFor(id)) {
            lane->Add(id);
        }

        QueuedEvent event;
        event.type       = NetEventType::Connected;
        event.connection = id;
        PushEvent(std::move(event));
    }

    void NetworkHost::OnDisconnected(ConnectionId id, DisconnectReason reason)
    {
        connected.erase(id);
        if (auto *lane = LaneFor(id)) {
            lane->Remove(id);
        }
        lastActivityMs.erase(id);
        lastHeartbeatMs.erase(id);
        pendingTokens.erase(id);

        auto sessionIt = connectionSessions.find(id);
        if (sessionIt != connectionSessions.end()) {
            auto connectionsIt = sessionConnections.find(sessionIt->second.value);
            if (connectionsIt != sessionConnections.end()) {
                auto &connections = connectionsIt->second;
                connections.erase(std::remove(connections.begin(), connections.end(), id), connections.end());
            }
        }
        connectionBackends.erase(id);

        if (!server && config.autoReconnect && hasConnectTarget &&
            reason != DisconnectReason::LocalClose && reason != DisconnectReason::Redirect) {
            nextReconnectAtMs = Now();
        }

        QueuedEvent event;
        event.type       = NetEventType::Disconnected;
        event.connection = id;
        event.reason     = reason;
        PushEvent(std::move(event));
    }

    void NetworkHost::OnMessage(ConnectionId id, ChannelId channel, MessageSequence sequence,
                                std::span<const uint8_t> payload)
    {
        lastActivityMs[id] = Now();
        bytesIn += payload.size();
        if (auto *lane = LaneFor(id)) {
            lane->bytesIn += payload.size();
        }

        QueuedEvent event;
        event.type       = NetEventType::Message;
        event.connection = id;
        event.channel    = channel;
        event.sequence   = sequence;
        event.payload.assign(payload.begin(), payload.end());
        PushEvent(std::move(event));
    }

    void NetworkHost::OnError(ConnectionId id, NetResult error)
    {
        QueuedEvent event;
        event.type       = NetEventType::Error;
        event.connection = id;
        event.error      = error;
        PushEvent(std::move(event));
    }

} // namespace sky::net

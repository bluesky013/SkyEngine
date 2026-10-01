//
// Created on 2026/09/25.
//

#pragma once

#include <network/replication/IReplicationSource.h>
#include <network/replication/ReplicationSnapshot.h>
#include <network/replication/ReplicationTick.h>

#include <network/ConnectionId.h>
#include <network/NetworkHost.h>

#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace sky::net {

    // Binds the replication algorithm to a NetworkHost: the server builds per-connection snapshots on a
    // fixed tick and sends them on the unreliable state channel; the client applies them, acknowledges
    // on the reliable event channel, and handles spawn/despawn events.
    class ReplicationHost {
    public:
        ReplicationHost(NetworkHost &host, IReplicationSource &source, const ReplicationConfig &config = {});
        ~ReplicationHost();

        void SetServer(bool isServer) { server = isServer; }

        void Update(double deltaSeconds);

        // Server: broadcast a spawn/despawn event on the reliable event channel.
        void BroadcastSpawn(ReplicatedEntityId entity, ReplicationTypeId type);
        void BroadcastDespawn(ReplicatedEntityId entity, ReplicationTypeId type);

        SnapshotSequence LastAppliedSequence() const { return lastApplied; }
        uint32_t         ConnectionCount() const { return static_cast<uint32_t>(connections.size()); }
        uint64_t         TotalTicks() const { return tick.TotalTicks(); }
        std::vector<ConnectionId> GetConnections() const;

    private:
        void HandleConnect(ConnectionId id);
        void HandleDisconnect(ConnectionId id);
        void HandleMessage(ConnectionId id, ChannelId channel, MessageSequence sequence, std::span<const uint8_t> payload);

        NetworkHost       &host;
        IReplicationSource &source;
        ReplicationConfig   config;
        bool                server = false;
        ReplicationTick     tick;

        SnapshotSequence lastApplied = 0;
        ConnectionId     clientConnection = INVALID_CONNECTION_ID;
        std::unordered_map<ConnectionId, SnapshotCodec::ConnectionState, ConnectionIdHash> connections;
        EncodedRecordCache encodeCache;   // server-side: one encode per record per tick, shared by all connections
    };

} // namespace sky::net

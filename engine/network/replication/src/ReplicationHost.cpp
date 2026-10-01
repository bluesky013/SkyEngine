//
// Created on 2026/09/25.
//

#include <network/replication/ReplicationHost.h>

namespace sky::net {

    ReplicationHost::ReplicationHost(NetworkHost &host, IReplicationSource &source, const ReplicationConfig &config)
        : host(host), source(source), config(config), tick(config.tickRateHz)
    {
        host.SetConnectHandler([this](ConnectionId id) { HandleConnect(id); });
        host.SetDisconnectHandler([this](ConnectionId id, DisconnectReason) { HandleDisconnect(id); });
        host.SetMessageHandler([this](ConnectionId id, ChannelId channel, MessageSequence sequence,
                                      std::span<const uint8_t> payload) {
            HandleMessage(id, channel, sequence, payload);
        });
    }

    ReplicationHost::~ReplicationHost() = default;

    std::vector<ConnectionId> ReplicationHost::GetConnections() const
    {
        std::vector<ConnectionId> result;
        result.reserve(connections.size());
        for (const auto &entry : connections) {
            result.push_back(entry.first);
        }
        return result;
    }

    void ReplicationHost::Update(double deltaSeconds)
    {
        if (!server) {
            return;
        }
        const uint32_t ticks = tick.Advance(deltaSeconds);
        for (uint32_t i = 0; i < ticks; ++i) {
            for (auto &entry : connections) {
                std::vector<std::vector<uint8_t>> messages;
                const SnapshotSequence firstSequence = entry.second.lastSent + 1;
                SnapshotCodec::BuildStateMessages(source, entry.second, config, firstSequence, entry.first, messages,
                                                  &encodeCache);
                for (auto &message : messages) {
                    if (message.size() > 8) {   // header is 8 bytes (type+flags+seq+count)
                        host.Send(entry.first, REPLICATION_STATE_CHANNEL, message, DeliveryMode::UnreliableSequenced);
                    }
                }
            }
        }
    }

    void ReplicationHost::BroadcastSpawn(ReplicatedEntityId entity, ReplicationTypeId type)
    {
        IReplicationRecord *record = source.FindReplica(entity, type);
        if (record == nullptr) {
            return;
        }
        std::vector<uint8_t> full;
        record->Encode(full);
        std::vector<uint8_t> message;
        SnapshotCodec::BuildSpawn(entity, type, full, message);
        for (auto &entry : connections) {
            host.Send(entry.first, REPLICATION_EVENT_CHANNEL, message, DeliveryMode::ReliableOrdered);
        }
    }

    void ReplicationHost::BroadcastDespawn(ReplicatedEntityId entity, ReplicationTypeId type)
    {
        encodeCache.Erase(EncodedRecordCache::MakeKey(entity, type));
        std::vector<uint8_t> message;
        SnapshotCodec::BuildDespawn(entity, type, message);
        for (auto &entry : connections) {
            host.Send(entry.first, REPLICATION_EVENT_CHANNEL, message, DeliveryMode::ReliableOrdered);
        }
    }

    void ReplicationHost::HandleConnect(ConnectionId id)
    {
        if (server) {
            connections[id];
        } else {
            clientConnection = id;
        }
    }

    void ReplicationHost::HandleDisconnect(ConnectionId id)
    {
        connections.erase(id);
        if (id == clientConnection) {
            clientConnection = INVALID_CONNECTION_ID;
        }
    }

    void ReplicationHost::HandleMessage(ConnectionId id, ChannelId channel, MessageSequence /*sequence*/,
                                        std::span<const uint8_t> payload)
    {
        if (server) {
            if (channel != REPLICATION_EVENT_CHANNEL) {
                return;
            }
            SnapshotSequence ack = 0;
            if (SnapshotCodec::ParseAck(payload, ack)) {
                auto it = connections.find(id);
                if (it != connections.end()) {
                    SnapshotCodec::Acknowledge(it->second, ack);
                }
            }
            return;
        }

        if (channel == REPLICATION_STATE_CHANNEL) {
            SnapshotSequence sequence = 0;
            bool full = false;
            if (!SnapshotCodec::ParseState(payload, sequence, full)) {
                return;
            }
            // Gap detection: apply and ack only contiguous deltas; a full (repair) snapshot is always
            // accepted. A gap is left unacknowledged so the server's repair path kicks in.
            if (!full) {
                if (sequence <= lastApplied) {
                    return;                      // duplicate/old
                }
                if (sequence != lastApplied + 1) {
                    return;                      // gap: await repair, do not ack
                }
            }
            SnapshotSequence applied = 0;
            if (SnapshotCodec::ApplyState(source, payload, applied)) {
                lastApplied = applied;
                std::vector<uint8_t> ack;
                SnapshotCodec::BuildAck(applied, ack);
                if (IsValid(id)) {
                    host.Send(id, REPLICATION_EVENT_CHANNEL, ack, DeliveryMode::ReliableOrdered);
                }
            }
        } else if (channel == REPLICATION_EVENT_CHANNEL) {
            ReplicationMessage       message = ReplicationMessage::Ack;
            ReplicatedEntityId       entity = 0;
            ReplicationTypeId        type = 0;
            std::span<const uint8_t> recordData;
            if (!SnapshotCodec::ParseEvent(payload, message, entity, type, recordData)) {
                return;
            }
            if (message == ReplicationMessage::Spawn) {
                IReplicationRecord *record = source.FindReplica(entity, type);
                if (record == nullptr) {
                    record = source.CreateReplica(entity, type);
                }
                if (record != nullptr) {
                    record->Apply(recordData);
                }
            } else if (message == ReplicationMessage::Despawn) {
                source.DestroyReplica(entity, type);
            }
        }
    }

} // namespace sky::net

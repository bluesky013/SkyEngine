//
// Created on 2026/09/25.
//

#include <network/ecs/EcsReplicationSource.h>

namespace sky::net {

    void EcsReplicationSource::ForEachRecord(const std::function<void(IReplicationRecord &)> &fn)
    {
        for (auto &adapter : adapters) {
            EcsReplicationRecord record;
            record.adapter = adapter.get();
            adapter->ForEachEntity([&](EntityId id) {
                record.entity = id;
                fn(record);
            });
        }
    }

    IReplicationRecord *EcsReplicationSource::CreateReplica(ReplicatedEntityId entity, ReplicationTypeId type)
    {
        auto it = byType.find(type);
        if (it == byType.end()) {
            return nullptr;
        }
        const EntityId id = static_cast<EntityId>(entity);
        if (!it->second->Contains(id)) {
            it->second->AddDefault(id);
        }
        scratch.adapter = it->second;
        scratch.entity  = id;
        return &scratch;
    }

    void EcsReplicationSource::DestroyReplica(ReplicatedEntityId entity, ReplicationTypeId type)
    {
        auto it = byType.find(type);
        if (it != byType.end()) {
            it->second->RemoveEntity(static_cast<EntityId>(entity));
        }
    }

    IReplicationRecord *EcsReplicationSource::FindReplica(ReplicatedEntityId entity, ReplicationTypeId type)
    {
        auto it = byType.find(type);
        if (it == byType.end()) {
            return nullptr;
        }
        const EntityId id = static_cast<EntityId>(entity);
        if (!it->second->Contains(id)) {
            return nullptr;
        }
        scratch.adapter = it->second;
        scratch.entity  = id;
        return &scratch;
    }

} // namespace sky::net

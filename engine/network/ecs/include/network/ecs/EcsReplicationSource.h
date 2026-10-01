//
// Created on 2026/09/25.
//

#pragma once

#include <network/replication/IReplicationSource.h>

#include <core/ecs/EntityRegistry.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

namespace sky::net {

    // Type-erased adapter over one ECS component pool.
    class EcsPoolAdapterBase {
    public:
        virtual ~EcsPoolAdapterBase() = default;

        virtual ReplicationTypeId TypeId() const = 0;
        virtual void ForEachEntity(const std::function<void(EntityId)> &fn) = 0;
        virtual bool Contains(EntityId entity) const = 0;
        virtual void AddDefault(EntityId entity) = 0;
        virtual void RemoveEntity(EntityId entity) = 0;
        virtual void Encode(EntityId entity, std::vector<uint8_t> &out) const = 0;
        virtual void Apply(EntityId entity, std::span<const uint8_t> data) = 0;

        // Field-level delta support.
        virtual uint32_t FieldCount() const { return 0; }
        virtual void EncodeField(EntityId, uint32_t, std::vector<uint8_t> &) const {}
        virtual void ApplyField(EntityId, uint32_t, std::span<const uint8_t>) {}
    };

    // Lightweight record returned through the seam; reused per iteration/call.
    class EcsReplicationRecord : public IReplicationRecord {
    public:
        EcsPoolAdapterBase *adapter = nullptr;
        EntityId            entity  = INVALID_ENTITY;

        ReplicatedEntityId Entity() const override { return static_cast<ReplicatedEntityId>(entity); }
        ReplicationTypeId  Type() const override { return adapter->TypeId(); }
        void Encode(std::vector<uint8_t> &out) const override { adapter->Encode(entity, out); }
        FieldMask DirtyMask() const override { return 0; }
        void Apply(std::span<const uint8_t> data) override { adapter->Apply(entity, data); }
        uint32_t FieldCount() const override { return adapter->FieldCount(); }
        void EncodeField(uint32_t index, std::vector<uint8_t> &out) const override
        {
            adapter->EncodeField(entity, index, out);
        }
        void ApplyField(uint32_t index, std::span<const uint8_t> data) override
        {
            adapter->ApplyField(entity, index, data);
        }
    };

    // Templated pool adapter. Encode/apply are supplied by the caller so the layer stays reflection-free.
    template <typename T>
    class EcsPoolAdapter : public EcsPoolAdapterBase {
    public:
        using EncodeFn      = std::function<void(const T &, std::vector<uint8_t> &)>;
        using ApplyFn       = std::function<void(T &, std::span<const uint8_t>)>;
        using FieldEncodeFn = std::function<void(const T &, std::vector<uint8_t> &)>;
        using FieldApplyFn  = std::function<void(T &, std::span<const uint8_t>)>;

        EcsPoolAdapter(EntityRegistry &registry, ReplicationTypeId type, EncodeFn encode, ApplyFn apply)
            : registry(registry), type(type), encode(std::move(encode)), apply(std::move(apply))
        {
        }

        EcsPoolAdapter(EntityRegistry &registry, ReplicationTypeId type, std::vector<FieldEncodeFn> encoders,
                       std::vector<FieldApplyFn> appliers)
            : registry(registry), type(type), fieldEncoders(std::move(encoders)), fieldAppliers(std::move(appliers))
        {
        }

        ReplicationTypeId TypeId() const override { return type; }

        void ForEachEntity(const std::function<void(EntityId)> &fn) override
        {
            auto &pool = registry.Pool<T>();
            std::vector<EntityId> ids;
            ids.reserve(pool.Size());
            for (uint32_t i = 0; i < pool.Size(); ++i) {
                ids.push_back(pool.DenseEntity(i));
            }
            // Deterministic order independent of dense layout (SparseSet swap-remove reorders storage).
            std::sort(ids.begin(), ids.end());
            for (EntityId id : ids) {
                if (pool.Contains(id)) {
                    fn(id);
                }
            }
        }

        bool Contains(EntityId entity) const override { return registry.Pool<T>().Contains(entity); }
        void AddDefault(EntityId entity) override { registry.Pool<T>().Add(entity, T{}); }
        void RemoveEntity(EntityId entity) override { registry.Pool<T>().Remove(entity); }

        void Encode(EntityId entity, std::vector<uint8_t> &out) const override
        {
            const T *component = registry.Pool<T>().Get(entity);
            if (component != nullptr) {
                encode(*component, out);
            }
        }

        void Apply(EntityId entity, std::span<const uint8_t> data) override
        {
            T *component = registry.Pool<T>().Get(entity);
            if (component != nullptr) {
                apply(*component, data);
            }
        }

        uint32_t FieldCount() const override { return static_cast<uint32_t>(fieldEncoders.size()); }

        void EncodeField(EntityId entity, uint32_t index, std::vector<uint8_t> &out) const override
        {
            const T *component = registry.Pool<T>().Get(entity);
            if (component != nullptr && index < fieldEncoders.size()) {
                fieldEncoders[index](*component, out);
            }
        }

        void ApplyField(EntityId entity, uint32_t index, std::span<const uint8_t> data) override
        {
            T *component = registry.Pool<T>().Get(entity);
            if (component != nullptr && index < fieldAppliers.size()) {
                fieldAppliers[index](*component, data);
            }
        }

    private:
        EntityRegistry   &registry;
        ReplicationTypeId type;
        EncodeFn          encode;
        ApplyFn           apply;
        std::vector<FieldEncodeFn> fieldEncoders;
        std::vector<FieldApplyFn>  fieldAppliers;
    };

    // Data-oriented replication source over an ECS registry. Registered component types are iterated
    // densely and batched, with a stable, layout-independent order.
    class EcsReplicationSource : public IReplicationSource {
    public:
        explicit EcsReplicationSource(EntityRegistry &registry) : registry(registry) {}

        template <typename T>
        void Register(ReplicationTypeId type, typename EcsPoolAdapter<T>::EncodeFn encode,
                      typename EcsPoolAdapter<T>::ApplyFn apply)
        {
            auto adapter = std::make_unique<EcsPoolAdapter<T>>(registry, type, std::move(encode), std::move(apply));
            byType[type] = adapter.get();
            adapters.push_back(std::move(adapter));
        }

        // Register a component with explicit replicated fields; the layer then sends field-level deltas.
        template <typename T>
        void RegisterFields(ReplicationTypeId type, std::vector<typename EcsPoolAdapter<T>::FieldEncodeFn> encoders,
                            std::vector<typename EcsPoolAdapter<T>::FieldApplyFn> appliers)
        {
            auto adapter = std::make_unique<EcsPoolAdapter<T>>(registry, type, std::move(encoders), std::move(appliers));
            byType[type] = adapter.get();
            adapters.push_back(std::move(adapter));
        }

        void ForEachRecord(const std::function<void(IReplicationRecord &)> &fn) override;
        IReplicationRecord *CreateReplica(ReplicatedEntityId entity, ReplicationTypeId type) override;
        void DestroyReplica(ReplicatedEntityId entity, ReplicationTypeId type) override;
        IReplicationRecord *FindReplica(ReplicatedEntityId entity, ReplicationTypeId type) override;

        // Per-connection interest/priority (area of interest).
        using InterestFn = std::function<bool(const IReplicationRecord &, ConnectionId)>;
        using PriorityFn = std::function<float(const IReplicationRecord &, ConnectionId)>;
        void SetInterestFilter(InterestFn fn) { interest = std::move(fn); }
        void SetPriorityProvider(PriorityFn fn) { priority = std::move(fn); }

        bool IsRelevantFor(const IReplicationRecord &record, ConnectionId connection) const override
        {
            return interest ? interest(record, connection)
                            : IReplicationSource::IsRelevantFor(record, connection);
        }
        float PriorityFor(const IReplicationRecord &record, ConnectionId connection) const override
        {
            return priority ? priority(record, connection)
                            : IReplicationSource::PriorityFor(record, connection);
        }

    private:
        EntityRegistry &registry;
        std::vector<std::unique_ptr<EcsPoolAdapterBase>> adapters;
        std::unordered_map<ReplicationTypeId, EcsPoolAdapterBase *> byType;
        mutable EcsReplicationRecord scratch;
        InterestFn interest;
        PriorityFn priority;
    };

} // namespace sky::net

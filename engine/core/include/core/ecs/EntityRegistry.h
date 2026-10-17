//
// Entity registry: entity lifecycle (free list + generation) + typed sparse-set pools.
//

#pragma once

#include <core/ecs/SparseSet.h>
#include <core/ecs/TypeId.h>
#include <core/platform/Platform.h>

#include <memory>
#include <unordered_map>
#include <vector>

namespace sky {

    class EntityRegistry {
    public:
        EntityRegistry()  = default;
        ~EntityRegistry() = default;

        EntityRegistry(const EntityRegistry &)            = delete;
        EntityRegistry &operator=(const EntityRegistry &) = delete;

        EntityId CreateEntity()
        {
            if (!mFreeList.empty()) {
                const uint32_t index = mFreeList.back();
                mFreeList.pop_back();
                mAlive[index] = true;
                return MakeEntityId(index, mGenerations[index]);
            }
            if (mGenerations.size() >= ECS_INDEX_MASK) {
                return INVALID_ENTITY;
            }
            const uint32_t index = static_cast<uint32_t>(mGenerations.size());
            mAlive.resize(index + 1, false);
            mGenerations.push_back(0);
            mAlive[index] = true;
            return MakeEntityId(index, 0);
        }

        void DestroyEntity(EntityId id)
        {
            if (!IsAlive(id)) {
                return;
            }
            const uint32_t index = GetEntityIndex(id);
            for (auto &[type, pool] : mPools) {
                pool->Remove(id);
            }
            if (mGenerations[index] < ECS_GENERATION_MASK) {
                mFreeList.push_back(index);
            }
            mAlive[index] = false;
            // Exhausted slots retire permanently; generation wrap could revive stale handles.
            if (mGenerations[index] < ECS_GENERATION_MASK) {
                ++mGenerations[index];
            }
        }

        bool IsAlive(EntityId id) const
        {
            const uint32_t index = GetEntityIndex(id);
            return IsValidEntity(id) && index < mGenerations.size() && mAlive[index] && mGenerations[index] == GetEntityGeneration(id);
        }

        template <typename T>
        SparseSet<T> &Pool()
        {
            const uint32_t id = TypeId<T>();
            auto           it = mPools.find(id);
            if (it == mPools.end()) {
                it = mPools.emplace(id, std::make_unique<PoolHolder<T>>()).first;
            } else {
                // debug: hash collision guard (same id, different tag)
                SKY_ASSERT(it->second->tag == TypeTag<T>() && "ECS type id hash collision");
            }
            return static_cast<PoolHolder<T> *>(it->second.get())->pool;
        }

        template <typename T>
        T *Add(EntityId id, T value)
        {
            if (!IsAlive(id)) {
                return nullptr;
            }
            return Pool<T>().Add(id, std::move(value));
        }

        template <typename T>
        T *Get(EntityId id)
        {
            if (!IsAlive(id)) {
                return nullptr;
            }
            return const_cast<T *>(std::as_const(*this).Get<T>(id));
        }

        template <typename T>
        const T *Get(EntityId id) const
        {
            if (!IsAlive(id)) {
                return nullptr;
            }
            const uint32_t typeId = TypeId<T>();
            auto           it     = mPools.find(typeId);
            if (it == mPools.end()) {
                return nullptr;
            }
            SKY_ASSERT(it->second->tag == TypeTag<T>() && "ECS type id hash collision");
            return static_cast<const PoolHolder<T> *>(it->second.get())->pool.Get(id);
        }

        template <typename T>
        void Remove(EntityId id)
        {
            if (!IsAlive(id)) {
                return;
            }
            const auto it = mPools.find(TypeId<T>());
            if (it != mPools.end()) {
                SKY_ASSERT(it->second->tag == TypeTag<T>() && "ECS type id hash collision");
                it->second->Remove(id);
            }
        }

        template <typename... Ts>
        auto View();

    private:
        struct PoolHolderBase {
            virtual ~PoolHolderBase()            = default;
            virtual void     Remove(EntityId id) = 0;
            std::string_view tag;
        };

        template <typename T>
        struct PoolHolder : PoolHolderBase {
            PoolHolder()
            {
                tag = TypeTag<T>();
            }
            void Remove(EntityId id) override
            {
                pool.Remove(id);
            }
            SparseSet<T> pool;
        };

        std::vector<uint32_t> mGenerations;
        std::vector<bool>     mAlive;
        std::vector<uint32_t> mFreeList;

        std::unordered_map<uint32_t, std::unique_ptr<PoolHolderBase>> mPools;
    };

} // namespace sky

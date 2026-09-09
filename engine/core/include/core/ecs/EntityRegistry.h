//
// Entity registry: entity lifecycle (free list + generation) + typed sparse-set pools.
//

#pragma once

#include <core/ecs/SparseSet.h>

#include <memory>
#include <unordered_map>
#include <vector>

namespace sky {

    // static auto-increment type id for component pools
    namespace detail {
        inline uint32_t NextTypeId()
        {
            static uint32_t counter = 0;
            return counter++;
        }
    } // namespace detail

    template <typename T>
    uint32_t TypeId()
    {
        static const uint32_t id = detail::NextTypeId();
        return id;
    }

    class EntityRegistry {
    public:
        EntityRegistry() = default;
        ~EntityRegistry() = default;

        EntityRegistry(const EntityRegistry &) = delete;
        EntityRegistry &operator=(const EntityRegistry &) = delete;

        EntityId CreateEntity()
        {
            if (!mFreeList.empty()) {
                const uint32_t index = mFreeList.back();
                mFreeList.pop_back();
                return MakeEntityId(index, mGenerations[index]);
            }
            const uint32_t index = static_cast<uint32_t>(mGenerations.size());
            mGenerations.push_back(0);
            return MakeEntityId(index, 0);
        }

        void DestroyEntity(EntityId id)
        {
            if (!IsAlive(id)) {
                return;
            }
            const uint32_t index = GetEntityIndex(id);
            ++mGenerations[index];
            mFreeList.push_back(index);
        }

        bool IsAlive(EntityId id) const
        {
            const uint32_t index = GetEntityIndex(id);
            return index < mGenerations.size() && mGenerations[index] == GetEntityGeneration(id);
        }

        template <typename T>
        SparseSet<T> &Pool()
        {
            const uint32_t id = TypeId<T>();
            auto it = mPools.find(id);
            if (it == mPools.end()) {
                it = mPools.emplace(id, std::make_unique<PoolHolder<T>>()).first;
            }
            return static_cast<PoolHolder<T> *>(it->second.get())->pool;
        }

        template <typename T>
        T &Add(EntityId id, T value)
        {
            return Pool<T>().Add(id, std::move(value));
        }

        template <typename T>
        T *Get(EntityId id)
        {
            if (!IsAlive(id)) {
                return nullptr;
            }
            return Pool<T>().Get(id);
        }

        template <typename T>
        const T *Get(EntityId id) const
        {
            if (!IsAlive(id)) {
                return nullptr;
            }
            const uint32_t typeId = TypeId<T>();
            auto it = mPools.find(typeId);
            if (it == mPools.end()) {
                return nullptr;
            }
            return static_cast<const PoolHolder<T> *>(it->second.get())->pool.Get(id);
        }

        template <typename T>
        void Remove(EntityId id)
        {
            Pool<T>().Remove(id);
        }

    private:
        struct PoolHolderBase {
            virtual ~PoolHolderBase() = default;
        };

        template <typename T>
        struct PoolHolder : PoolHolderBase {
            SparseSet<T> pool;
        };

        std::vector<uint32_t> mGenerations;
        std::vector<uint32_t> mFreeList;

        std::unordered_map<uint32_t, std::unique_ptr<PoolHolderBase>> mPools;
    };

} // namespace sky

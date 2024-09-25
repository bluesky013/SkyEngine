//
// Created by blues on 2024/4/19.
//

#pragma once

#include <cstdint>
#include <vector>
#include <core/environment/Singleton.h>

namespace sky {

    struct EntityValues {
        uint64_t valid : 1;
        uint64_t id    : 63;
    };
    static_assert(sizeof(EntityValues) == 8);

    using EntityId = uint64_t;
    struct Entity {
        union {
            EntityValues value;
            EntityId     id;
        };
    };

    class EntityManager : Singleton<EntityManager> {
    public:
        EntityManager() = default;
        ~EntityManager() override = default;

        template <typename T, typename ...Args>
        T* AddComponent(EntityId entity, Args&&... args)
        {
            return nullptr;
        }

        template <typename T>
        T* GetComponent(EntityId entity)
        {
            return nullptr;
        }

        template <typename T>
        void RemoveComponent(EntityId entity)
        {
        }

        EntityId CreateEntity();
        void DestroyEntity(EntityId entity);
    private:
        std::vector<Entity> entities;
        std::vector<EntityId> freeList;

        using Storage = std::vector<uint8_t>;
        std::unordered_map<uint32_t, Storage> storages;
    };

} // namespace sky

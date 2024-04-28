//
// Created by blues on 2024/4/19.
//

#include <framework/World/Entity.h>

namespace sky {

    EntityId EntityManager::CreateEntity()
    {
        if (freeList.empty()) {
            Entity entity = {};
            entity.value.valid = true;
            entity.value.id = static_cast<uint64_t>(entities.size());
            entities.emplace_back(entity);
            return entity.id;
        }

        auto back = freeList.back();
        entities[back].value.valid = true;
        freeList.pop_back();
        return back;
    }

    void EntityManager::DestroyEntity(EntityId entity)
    {
        entities[entity].value.valid = false;
        freeList.emplace_back(entity);
    }

} // namespace sky
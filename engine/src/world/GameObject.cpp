//
// Created by Zach Lee on 2021/11/13.
//


#include <engine/world/GameObject.h>
#include <engine/world/World.h>
#include <engine/world/Component.h>

namespace sky {

    GameObject::~GameObject()
    {
        if (world != nullptr) {
            world->RemoveGameObject(this);
        }
        for (auto& pair : components) {
            delete pair.second;
        }
        components.clear();
    }

    uint32_t GameObject::GetId() const
    {
        return objId;
    }

    const std::string& GameObject::GetName() const
    {
        return name;
    }
}
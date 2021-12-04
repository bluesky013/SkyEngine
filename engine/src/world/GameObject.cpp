//
// Created by Zach Lee on 2021/11/13.
//


#include <world/GameObject.h>
#include <world/World.h>
#include <world/Component.h>

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
//
// Created by Zach Lee on 2021/11/13.
//


#include <world/GameObject.h>
#include <world/World.h>
#include <world/Component.h>

namespace sky {

    GameObject::~GameObject()
    {
        world->RemoveGameObject(this);
        for (auto& comp : components) {
            comp->object = nullptr;
        }
    }

    void GameObject::AddComponent(Component* component)
    {
        auto iter = std::find(components.begin(), components.end(), component);
        if (iter != components.end()) {
            return;
        }
        components.emplace_back(component);
        component->object = this;
    }

    void GameObject::RemoveComponent(Component* component)
    {
        auto iter = std::find(components.begin(), components.end(), component);
        if (iter == components.end()) {
            return;
        }
        components.erase(iter);
        component->object = nullptr;
    }

    uint32_t GameObject::GetId() const
    {
        std::hash<uint32_t>;
        return objId;
    }
}
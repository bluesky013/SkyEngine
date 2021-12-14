//
// Created by Zach Lee on 2021/11/13.
//


#include <engine/world/GameObject.h>
#include <engine/world/World.h>
#include <engine/world/Component.h>
#include <engine/world/TransformComponent.h>

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

    void GameObject::SetParent(GameObject* gameObject)
    {
        auto trans = GetComponent<TransformComponent>();
        auto parent = gameObject == nullptr ? world->GetRoot()->GetComponent<TransformComponent>()
            : gameObject->GetComponent<TransformComponent>();
        trans->SetParent(parent);
    }

    GameObject::ComponentMap& GameObject::GetComponents()
    {
        return components;
    }
}
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
        for (auto iter : components) {
            delete iter;
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

    void GameObject::Tick(float time)
    {
        for (auto& comp : components) {
            comp->OnTick(time);
        }
    }

    GameObject::ComponentList& GameObject::GetComponents()
    {
        return components;
    }
}
//
// Created by Zach Lee on 2021/11/13.
//

#include <engine/world/Component.h>
#include <engine/world/GameObject.h>
#include <engine/world/TransformComponent.h>
#include <engine/world/World.h>

namespace sky {

    GameObject::~GameObject()
    {
        for (auto comp : components) {
            comp->~Component();
            resource->deallocate(comp, comp->GetTypeInfo()->size);
        }
        components.clear();
    }

    uint32_t GameObject::GetId() const
    {
        return objId;
    }

    const std::string &GameObject::GetName() const
    {
        return name;
    }

    World *GameObject::GetWorld() const
    {
        return world;
    }

    void GameObject::SetParent(GameObject *gameObject)
    {
        auto trans = GetComponent<TransformComponent>();
        if (gameObject == nullptr) {
            gameObject = world->GetRoot();
        }
        TransformComponent *parent = gameObject != nullptr ? gameObject->GetComponent<TransformComponent>() : nullptr;
        trans->SetParent(parent);
    }

    void GameObject::Tick(float time)
    {
        for (auto &comp : components) {
            comp->OnTick(time);
        }
    }

    GameObject::ComponentList &GameObject::GetComponents()
    {
        return components;
    }
} // namespace sky

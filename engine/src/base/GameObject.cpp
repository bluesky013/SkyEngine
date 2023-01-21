//
// Created by Zach Lee on 2021/11/13.
//

#include <engine/base/GameObject.h>
#include <engine/world/TransformComponent.h>
#include <engine/world/World.h>

#include <framework/serialization/SerializationContext.h>
#include <framework/serialization/JsonArchive.h>
namespace sky {

    void GameObject::Reflect()
    {
        SerializationContext::Get()->Register<GameObject>("GameObject")
            .JsonLoad<&GameObject::Load>()
            .JsonSave<&GameObject::Save>();
    }

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

    void GameObject::SetName(const std::string &name_)
    {
        name = name_;
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

    void GameObject::SetParent(const Uuid &gameObject)
    {
        SetParent(world->GetGameObjectByUuid(gameObject));
    }

    GameObject *GameObject::GetParent() const
    {
        auto trans = GetComponent<TransformComponent>()->GetParent();
        return trans != nullptr ? trans->object : nullptr;
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

    void GameObject::Save(JsonOutputArchive &ar) const
    {
        ar.Key("uuid");
        ar.SaveValue(uuid.ToString());

        ar.Key("parent");
        ar.SaveValue(GetParent()->GetUuid().ToString());

        ar.Key("name");
        ar.SaveValue(name);

        ar.Key("components");
        ar.StartArray();
        for (auto &comp : components) {
            ar.SaveValueObject(*comp);
        }
        ar.EndArray();
    }

    void GameObject::Load(JsonInputArchive &ar)
    {

    }

} // namespace sky

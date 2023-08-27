//
// Created by Zach Lee on 2021/11/13.
//

#include <framework/world/GameObject.h>
#include <framework/world/TransformComponent.h>
#include <framework/world/World.h>

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
        ar.StartObject();
        ar.SaveValueObject("uuid", uuid.ToString());
        ar.SaveValueObject("parent", GetParent()->GetUuid().ToString());
        ar.SaveValueObject("name", name);

        ar.Key("components");
        ar.StartArray();
        for (auto &comp : components) {
            ar.StartObject();
            ar.Key("type");
            ar.SaveValue(comp->GetType());
            ar.Key("data");
            ar.SaveValueObject(*comp);
            ar.EndObject();
        }
        ar.EndArray();
        ar.EndObject();
    }

    void GameObject::Load(JsonInputArchive &ar)
    {
        std::string id;
        ar.LoadKeyValue("uuid", id);
        uuid = Uuid::CreateFromString(id.c_str());

        std::string parentId;
        ar.LoadKeyValue("parent", parentId);
        auto parent = Uuid::CreateFromString(parentId.c_str());

        ar.LoadKeyValue("name", name);

        uint32_t size = ar.StartArray("components");
        for (uint32_t i = 0; i < size; ++i) {
            uint32_t typeId = 0;
            ar.LoadKeyValue("type", typeId);
            auto *comp = AddComponent(typeId);
            ar.LoadKeyValue("data", *comp);
            ar.NextArrayElement();
        }
        ar.End();
        SetParent(parent);
    }

} // namespace sky

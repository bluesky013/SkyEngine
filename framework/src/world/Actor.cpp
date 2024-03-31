//
// Created by Zach Lee on 2021/11/13.
//

#include <framework/world/Actor.h>
#include <framework/world/TransformComponent.h>
#include <framework/world/World.h>

#include <framework/serialization/SerializationContext.h>
#include <framework/serialization/JsonArchive.h>
namespace sky {

    void Actor::Reflect()
    {
        SerializationContext::Get()->Register<Actor>("Actor")
            .JsonLoad<&Actor::Load>()
            .JsonSave<&Actor::Save>();
    }

    Actor::~Actor()
    {
        for (auto comp : components) {
            comp->~Component();
            resource->deallocate(comp, comp->GetTypeInfo()->size);
        }
        components.clear();
    }

    uint32_t Actor::GetId() const
    {
        return objId;
    }

    void Actor::SetName(const std::string &name_)
    {
        name = name_;
    }

    const std::string &Actor::GetName() const
    {
        return name;
    }

    World *Actor::GetWorld() const
    {
        return world;
    }

    void Actor::SetParent(Actor *actor)
    {
        auto trans = GetComponent<TransformComponent>();
        if (actor == nullptr) {
            actor = world->GetRoot();
        }
        TransformComponent *parent = actor != nullptr ? actor->GetComponent<TransformComponent>() : nullptr;
        trans->SetParent(parent);
    }

    void Actor::SetParent(const Uuid &actor)
    {
        SetParent(world->GetActorByUuid(actor));
    }

    Actor *Actor::GetParent() const
    {
        auto trans = GetComponent<TransformComponent>()->GetParent();
        return trans != nullptr ? trans->object : nullptr;
    }

    void Actor::Tick(float time)
    {
        for (auto &comp : components) {
            comp->OnTick(time);
        }
    }

    Actor::ComponentList &Actor::GetComponents()
    {
        return components;
    }

    void Actor::Save(JsonOutputArchive &ar) const
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

    void Actor::Load(JsonInputArchive &ar)
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

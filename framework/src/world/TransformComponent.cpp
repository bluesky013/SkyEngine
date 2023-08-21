//
// Created by Zach Lee on 2021/11/13.
//

#include <core/logger/Logger.h>
#include <framework/world/GameObject.h>
#include <framework/world/TransformComponent.h>
#include <framework/serialization/PropertyCommon.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/serialization/JsonArchive.h>
#include <string>

namespace sky {

    static const char *TAG = "TransformComponent";

    void TransformComponent::Reflect()
    {
        SerializationContext::Get()
            ->Register<TransformComponent>(NAME)
            .Member<&TransformComponent::local>("local")
            .Property(UI_LABEL_VISIBLE, false)
            .Member<&TransformComponent::world>("world")
            .Property(UI_PROP_VISIBLE, false);

        ComponentFactory::Get()->RegisterComponent<TransformComponent>();
    }

    TransformComponent::~TransformComponent()
    {
        if (parent != nullptr) {
            parent->children.erase(std::remove(parent->children.begin(), parent->children.end(), this), parent->children.end());
        }
        for (auto &child : children) {
            child->parent = nullptr;
        }
        children.clear();
    }

    void TransformComponent::SetParent(TransformComponent *newParent)
    {
        if (parent == newParent) {
            return;
        }

        if (parent != nullptr) {
            parent->children.erase(std::remove(parent->children.begin(), parent->children.end(), this), parent->children.end());
        }
        parent = newParent;

        if (parent != nullptr) {
            auto iter = std::find(parent->children.begin(), parent->children.end(), this);
            if (iter == parent->children.end()) {
                newParent->children.emplace_back(this);
            }
        }
        UpdateLocal();
    }

    TransformComponent *TransformComponent::GetParent() const
    {
        return parent;
    }

    const std::vector<TransformComponent *> &TransformComponent::GetChildren() const
    {
        return children;
    }

    void TransformComponent::PrintChild(TransformComponent &comp, std::string str)
    {
        LOG_I(TAG, "%s%s", str.c_str(), comp.object->GetName().c_str());
        for (auto &child : comp.children) {
            PrintChild(*child, str + "  ");
        }
    }

    void TransformComponent::Print()
    {
        PrintChild(*this, "");
    }

    void TransformComponent::TransformChanged()
    {
        for (auto child : children) {
            child->UpdateWorld();
        }
    }

    void TransformComponent::UpdateLocal()
    {
        if (parent != nullptr) {
            auto inverse = parent->world.GetInverse();
            local        = inverse * world;
        } else {
            local = world;
        }
        TransformChanged();
    }

    void TransformComponent::UpdateWorld()
    {
        world = GetParentTransform() * local;
        TransformChanged();
    }

    const Transform &TransformComponent::GetParentTransform() const
    {
        if (parent != nullptr) {
            return parent->world;
        }
        return Transform::GetIdentity();
    }

    void TransformComponent::SetWorldTranslation(const Vector3 &translation)
    {
        world.translation = translation;
        UpdateLocal();
    }

    void TransformComponent::SetWorldRotation(const Quaternion &rotation)
    {
        world.rotation = rotation;
        UpdateLocal();
    }

    void TransformComponent::SetWorldScale(const Vector3 &scale)
    {
        world.scale = scale;
        UpdateLocal();
    }

    void TransformComponent::SetLocalTranslation(const Vector3 &translation)
    {
        local.translation = translation;
        if (!suppressWorldChange) {
            UpdateWorld();
        }
    }

    void TransformComponent::SetLocalRotation(const Quaternion &rotation)
    {
        local.rotation = rotation;
        if (!suppressWorldChange) {
            UpdateWorld();
        }
    }

    void TransformComponent::SetLocalScale(const Vector3 &scale)
    {
        local.scale = scale;
        if (!suppressWorldChange) {
            UpdateWorld();
        }
    }

    const Transform &TransformComponent::GetLocal() const
    {
        return local;
    }

    const Transform &TransformComponent::GetWorld() const
    {
        return world;
    }

    void TransformComponent::Save(JsonOutputArchive &ar) const
    {
        ar.StartObject();
        ar.SaveValueObject("transform", local);
        ar.EndObject();
    }

    void TransformComponent::Load(JsonInputArchive &ar)
    {
        ar.LoadKeyValue("transform", local);
    }

} // namespace sky

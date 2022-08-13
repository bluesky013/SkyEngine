//
// Created by Zach Lee on 2021/11/13.
//

#include <engine/world/TransformComponent.h>
#include <engine/world/GameObject.h>
#include <core/logger/Logger.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/serialization/PropertyCommon.h>
#include <string>

namespace sky {

    static const char* TAG = "TransformComponent";

    void TransformComponent::Reflect()
    {
        SerializationContext::Get()->Register<TransformComponent>(TypeName())
            .Member<&TransformComponent::local>("local")
            .Member<&TransformComponent::world>("world")
            .Property(UI_PROP_VISIBLE, false);
    }

    TransformComponent::~TransformComponent()
    {
        if (parent != nullptr) {
            parent->children.erase(std::remove(parent->children.begin(), parent->children.end(), this), parent->children.end());
        }
        for (auto& child : children) {
            child->parent = nullptr;
        }
        children.clear();
    }

    void TransformComponent::SetParent(TransformComponent* newParent)
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

    TransformComponent* TransformComponent::GetParent() const
    {
        return parent;
    }

    const std::vector<TransformComponent*>& TransformComponent::GetChildren() const
    {
        return children;
    }

    void TransformComponent::PrintChild(TransformComponent& comp, std::string str)
    {
        LOG_I(TAG, "%s%s", str.c_str(), comp.object->GetName().c_str());
        for (auto& child : comp.children) {
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
            local = inverse * world;
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

    const Transform& TransformComponent::GetParentTransform() const
    {
        if (parent != nullptr) {
            return parent->world;
        }
        return Transform::GetIdentity();
    }

    void TransformComponent::SetWorldTranslation(const Vector3& translation)
    {
        world.translation = translation;
        UpdateLocal();
    }

    void TransformComponent::SetWorldRotation(const Quaternion& rotation)
    {
        world.rotation = rotation;
        UpdateLocal();
    }

    void TransformComponent::SetWorldScale(const Vector3& scale)
    {
        world.scale = scale;
        UpdateLocal();
    }

    void TransformComponent::SetLocalTranslation(const Vector3& translation)
    {
        local.translation = translation;
        if (!suppressWorldChange) {
            UpdateWorld();
        }
    }

    void TransformComponent::SetLocalRotation(const Quaternion& rotation)
    {
        local.rotation = rotation;
        if (!suppressWorldChange) {
            UpdateWorld();
        }
    }

    void TransformComponent::SetLocalScale(const Vector3& scale)
    {
        local.scale = scale;
        if (!suppressWorldChange) {
            UpdateWorld();
        }
    }

    const Transform& TransformComponent::GetLocal() const
    {
        return local;
    }

    const Transform& TransformComponent::GetWorld() const
    {
        return world;
    }
}
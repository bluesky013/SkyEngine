//
// Created by Zach Lee on 2021/11/13.
//

#include <framework/world/TransformComponent.h>
#include <framework/world/ComponentFactory.h>
#include <framework/world/Actor.h>
#include <framework/serialization/SerializationContext.h>

namespace sky {

    static const char *TAG = "TransformComponent";

    void TransformComponent::Reflect(SerializationContext *context)
    {
        context->Register<TransformData>("TransformData")
                .Member<&TransformData::local>("data")
                .Member<&TransformData::parent>("parent");

        REGISTER_BEGIN(TransformComponent, context)
                REGISTER_MEMBER(translation, SetLocalTranslation, GetLocalTranslation)
                REGISTER_MEMBER(rotation, SetLocalRotationEuler, GetLocalRotationEuler)
                REGISTER_MEMBER(scale, SetLocalScale, GetLocalScale);

        ComponentFactory::Get()->RegisterComponent<TransformComponent>("Base");
    }

    TransformComponent::~TransformComponent()
    {
        SetParent(nullptr);
        for (auto &child : children) {
            child->parent = nullptr;
        }
    }

    Matrix4 TransformComponent::GetWorldMatrix() const
    {
        return data.global.ToMatrix();
    }

    const Transform &TransformComponent::GetWorldTransform() const
    {
        return data.global;
    }

    void TransformComponent::SetParent(TransformComponent *parent_)
    {
        if (parent_ == parent || parent_ == this) {
            return;
        }

        if (parent != nullptr) {
            parent->children.erase(std::remove(parent->children.begin(), parent->children.end(), this), parent->children.end());
        }

        parent = parent_;
        data.parent = parent != nullptr ? parent->actor->GetUuid() : Uuid::GetEmpty();
        UpdateLocal();

        if (parent != nullptr) {
            parent->children.emplace_back(this);
        }
    }

    void TransformComponent::OnTransformChanged() // NOLINT
    {
        TransformEvent::BroadCast(actor, &ITransformEvent::OnTransformChanged, data.global, data.local);
        for (auto *child : children) {
            child->OnTransformChanged();
        }
    }

    void TransformComponent::SetWorldTransform(const Transform &trans)
    {
        data.global = trans;
        UpdateLocal();
        OnTransformChanged();
    }

    void TransformComponent::SetWorldTranslation(const Vector3 &translation)
    {
        data.global.translation = translation;
        UpdateLocal();
        OnTransformChanged();
    }
    void TransformComponent::SetWorldRotation(const Quaternion &rotation)
    {
        data.global.rotation = rotation;
        UpdateLocal();
        OnTransformChanged();
    }
    void TransformComponent::SetWorldScale(const Vector3 &scale)
    {
        data.global.scale = scale;
        UpdateLocal();
        OnTransformChanged();
    }
    void TransformComponent::SetLocalTransform(const Transform &trans)
    {
        data.local = trans;
        UpdateGlobal();
        OnTransformChanged();
    }
    void TransformComponent::SetLocalTranslation(const Vector3 &translation)
    {
        data.local.translation = translation;
        UpdateGlobal();
        OnTransformChanged();
    }
    void TransformComponent::SetLocalRotationEuler(const Vector3 &euler)
    {
        data.local.rotation.FromEulerYZX(euler);
        UpdateGlobal();
        OnTransformChanged();
    }
    void TransformComponent::SetLocalRotation(const Quaternion &rotation)
    {
        data.local.rotation = rotation;
        UpdateGlobal();
        OnTransformChanged();
    }
    void TransformComponent::SetLocalScale(const Vector3 &scale)
    {
        data.local.scale = scale;
        UpdateGlobal();
        OnTransformChanged();
    }

    Vector3 TransformComponent::GetLocalRotationEuler() const
    {
        return data.local.rotation.ToEulerYZX();
    }

    const Quaternion &TransformComponent::GetLocalRotation() const
    {
        return data.local.rotation;
    }
    const Vector3 &TransformComponent::GetLocalTranslation() const
    {
        return data.local.translation;
    }
    const Vector3 &TransformComponent::GetLocalScale() const
    {
        return data.local.scale;
    }

    void TransformComponent::UpdateLocal()
    {
        data.local = parent != nullptr ? parent->data.global.GetInverse() * data.global : data.global;
    }

    void TransformComponent::UpdateGlobal()
    {
        data.global = parent != nullptr ? parent->data.global * data.local * data.global : data.local;
    }

    void TransformComponent::OnSerialized()
    {
        UpdateGlobal();
    }
} // namespace sky

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
        if (parent != nullptr) {
            auto &siblings = parent->children;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
            parent = nullptr;
        }

        // Reparent children to the root before this component dies, so they neither keep a dangling
        // parent pointer nor a stale serialized parent id.
        auto childCopy = children;
        children.clear();
        for (auto *child : childCopy) {
            if (child != nullptr) {
                child->LinkParent(nullptr);
                child->UpdateGlobal();
            }
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

    bool TransformComponent::HasAncestor(const TransformComponent *target) const
    {
        for (auto *node = parent; node != nullptr; node = node->parent) {
            if (node == target) {
                return true;
            }
        }
        return false;
    }

    bool TransformComponent::LinkParent(TransformComponent *parent_)
    {
        if (parent_ == parent || parent_ == this) {
            return false;
        }
        // Reject links that would create a cycle (parent_ is already a descendant of this).
        if (parent_ != nullptr && parent_->HasAncestor(this)) {
            return false;
        }

        if (parent != nullptr) {
            auto &siblings = parent->children;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
        }

        parent = parent_;
        data.parent = (parent_ != nullptr && parent_->actor != nullptr) ? parent_->actor->GetUuid() : Uuid::GetEmpty();

        if (parent_ != nullptr) {
            parent_->children.emplace_back(this);
        }
        return true;
    }

    void TransformComponent::SetParent(TransformComponent *parent_)
    {
        // Preserve the current world transform (used for runtime re-parenting).
        if (LinkParent(parent_)) {
            UpdateLocal();
        }
    }

    void TransformComponent::SetParentPreserveLocal(TransformComponent *parent_)
    {
        // Preserve the authored/serialized local transform and derive the world transform.
        if (LinkParent(parent_)) {
            UpdateGlobal();
        }
    }

    void TransformComponent::OnTransformChanged() // NOLINT
    {
        TransformEvent::BroadCast(actor, &ITransformEvent::OnTransformChanged, data.global, data.local);
        for (auto *child : children) {
            if (child != nullptr) {
                child->data.global = child->parent != nullptr ? child->parent->data.global * child->data.local : child->data.local;
                child->OnTransformChanged();
            }
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
        data.global = parent != nullptr ? parent->data.global * data.local : data.local;
    }

    void TransformComponent::OnSerialized()
    {
        UpdateGlobal();
    }
} // namespace sky

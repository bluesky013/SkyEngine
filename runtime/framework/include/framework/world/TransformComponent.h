//
// Created by Zach Lee on 2021/11/13.
//

#pragma once

#include <core/math/Transform.h>
#include <core/event/Event.h>
#include <framework/world/Component.h>

namespace sky {

    struct TransformData {
        Transform local;
        Transform global;
        Uuid parent;
    };

    class TransformComponent : public ComponentAdaptor<TransformData> {
    public:
        TransformComponent() = default;
        ~TransformComponent() override;

        static void Reflect(SerializationContext *context);

        COMPONENT_RUNTIME_INFO(TransformComponent)

        void SetParent(TransformComponent *parent);
        TransformComponent *GetParent() const { return parent; }
        void OnTransformChanged();

        Matrix4 GetWorldMatrix() const;
        const Transform &GetWorldTransform() const;

        void SetWorldTransform(const Transform &trans);
        void SetWorldTranslation(const Vector3 &translation);
        void SetWorldRotation(const Quaternion &rotation);
        void SetWorldScale(const Vector3 &scale);

        void SetLocalTransform(const Transform &trans);
        void SetLocalTranslation(const Vector3 &translation);
        void SetLocalRotationEuler(const Vector3 &euler);
        void SetLocalRotation(const Quaternion &rotation);
        void SetLocalScale(const Vector3 &scale);

        const Transform& GetLocalTransform() const { return data.local; }

        Vector3 GetLocalRotationEuler() const;
        const Quaternion &GetLocalRotation() const;
        const Vector3 &GetLocalTranslation() const;
        const Vector3 &GetLocalScale() const;

    private:
        void UpdateLocal();
        void UpdateGlobal();
        void OnSerialized() override;

        TransformComponent* parent = nullptr;
        std::vector<TransformComponent*> children;
    };

} // namespace sky

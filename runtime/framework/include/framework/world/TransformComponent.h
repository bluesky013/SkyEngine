//
// Created by Zach Lee on 2021/11/13.
//

#pragma once

#include <core/math/Transform.h>
#include <framework/world/Component.h>

namespace sky {

    struct TransformData {
        Transform local;
        Transform global;
    };

    class ITransform {
    public:
        ITransform() = default;
        ~ITransform() = default;

    private:

    };

    class TransformComponent : public ComponentAdaptor<TransformData> {
    public:
        TransformComponent() = default;
        ~TransformComponent() override;

        static void Reflect(SerializationContext *context);

        COMPONENT_RUNTIME_INFO(TransformComponent)

        Matrix4 GetWorldMatrix() const;

        void SetWorldTranslation(const Vector3 &translation);
        void SetWorldRotation(const Quaternion &rotation);
        void SetWorldScale(const Vector3 &scale);

        void SetLocalTranslation(const Vector3 &translation);
        void SetLocalRotationEuler(const Vector3 &euler);
        void SetLocalRotation(const Quaternion &rotation);
        void SetLocalScale(const Vector3 &scale);

        Vector3 GetLocalRotationEuler() const;
        const Quaternion &GetLocalRotation() const { return data.local.rotation; }
        const Vector3 &GetLocalTranslation() const { return data.local.translation; }
        const Vector3 &GetLocalScale() const { return data.local.scale; }
    };

} // namespace sky

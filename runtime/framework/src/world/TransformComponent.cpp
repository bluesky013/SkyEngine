//
// Created by Zach Lee on 2021/11/13.
//

#include <core/logger/Logger.h>
#include <framework/world/TransformComponent.h>
#include <framework/serialization/PropertyCommon.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/serialization/JsonArchive.h>
#include <string>

namespace sky {

    static const char *TAG = "TransformComponent";

    void TransformComponent::Reflect(SerializationContext *context)
    {
        context->Register<TransformData>("TransformData")
                .Member<&TransformData::local>("data");

        REGISTER_BEGIN(TransformComponent, context)
                REGISTER_MEMBER(translation, SetLocalTranslation, GetLocalTranslation)
                REGISTER_MEMBER(rotation, SetLocalRotationEuler, GetLocalRotationEuler)
                REGISTER_MEMBER(scale, SetLocalScale, GetLocalScale);
    }

    TransformComponent::~TransformComponent()
    {
    }

    Matrix4 TransformComponent::GetWorldMatrix() const
    {
        return data.local.ToMatrix();
    }

    void TransformComponent::SetWorldTranslation(const Vector3 &translation)
    {

    }
    void TransformComponent::SetWorldRotation(const Quaternion &rotation)
    {

    }
    void TransformComponent::SetWorldScale(const Vector3 &scale)
    {

    }
    void TransformComponent::SetLocalTranslation(const Vector3 &translation)
    {
        data.local.translation = translation;
    }
    void TransformComponent::SetLocalRotationEuler(const Vector3 &euler)
    {
        data.local.rotation.FromEulerYZX(euler);
    }
    void TransformComponent::SetLocalRotation(const Quaternion &rotation)
    {
        data.local.rotation = rotation;
    }
    void TransformComponent::SetLocalScale(const Vector3 &scale)
    {
        data.local.scale = scale;
    }

    Vector3 TransformComponent::GetLocalRotationEuler() const
    {
        return data.local.rotation.ToEulerYZX();
    }
} // namespace sky

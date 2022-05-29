//
// Created by Zach Lee on 2021/12/15.
//

#include <engine/SkyEngine.h>
#include <engine/world/World.h>
#include <core/math/Math.h>
#include <core/math/Vector.h>
#include <core/math/Matrix.h>
#include <core/math/Quaternion.h>
#include <core/math/Transform.h>
#include <framework/serialization/SerializationContext.h>

namespace sky {

    void BasicReflect()
    {
        SerializationContext::Get()->Register<Vector2>("Vector2")
            .Member<&Vector2::x>("x")
            .Member<&Vector2::y>("y");

        SerializationContext::Get()->Register<Vector3>("Vector3")
            .Member<&Vector3::x>("x")
            .Member<&Vector3::y>("y")
            .Member<&Vector3::z>("z");

        SerializationContext::Get()->Register<Vector4>("Vector4")
            .Member<&Vector4::x>("x")
            .Member<&Vector4::y>("y")
            .Member<&Vector4::z>("z")
            .Member<&Vector4::w>("w");

        SerializationContext::Get()->Register<Quaternion>("Quaternion")
            .Member<&Quaternion::x>("x")
            .Member<&Quaternion::y>("y")
            .Member<&Quaternion::z>("z")
            .Member<&Quaternion::w>("w");

        SerializationContext::Get()->Register<Transform>("Transform")
            .Member<&Transform::translation>("translation")
            .Member<&Transform::scale>("scale")
            .Member<&Transform::rotation>("rotation");
    }

    void SkyEngine::Reflect()
    {
        World::Reflect();
        BasicReflect();
    }

}
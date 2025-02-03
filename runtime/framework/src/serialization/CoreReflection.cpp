//
// Created by Zach Lee on 2023/9/2.
//

#include <framework/serialization/CoreReflection.h>

#include <core/math/Matrix4.h>
#include <core/math/Vector2.h>
#include <core/math/Vector3.h>
#include <core/math/Vector4.h>
#include <core/math/Quaternion.h>
#include <core/math/Transform.h>
#include <core/math/Color.h>
#include <core/util/Uuid.h>

#include <framework/world/World.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/serialization/JsonArchive.h>
#include <framework/serialization/PropertyCommon.h>

namespace sky {
    template <uint32_t N>
    static void LoadN(float *v, JsonInputArchive &ar)
    {
        for (uint32_t i = 0; i < N; ++i) {
            ar.LoadArrayElement(v[i]);
        }
    }

    template <uint32_t N>
    static void SaveN(const float *v, JsonOutputArchive &ar)
    {
        ar.StartArray();
        for (uint32_t i = 0; i < N; ++i) {
            ar.SaveValue(v[i]);
        }
        ar.EndArray();
    }

    void JsonLoadVec2(Vector2 &val, JsonInputArchive &ar)    { LoadN<2>(val.v, ar); }
    void JsonLoadVec3(Vector3 &val, JsonInputArchive &ar)    { LoadN<3>(val.v, ar); }
    void JsonLoadVec4(Vector4 &val, JsonInputArchive &ar)    { LoadN<4>(val.v, ar); }
    void JsonLoadQuat(Quaternion &val, JsonInputArchive &ar) { LoadN<4>(val.v, ar); }

    void JsonSaveVec2(const Vector2 &val, JsonOutputArchive &ar)    { SaveN<2>(val.v, ar); }
    void JsonSaveVec3(const Vector3 &val, JsonOutputArchive &ar)    { SaveN<3>(val.v, ar); }
    void JsonSaveVec4(const Vector4 &val, JsonOutputArchive &ar)    { SaveN<4>(val.v, ar); }
    void JsonSaveQuat(const Quaternion &val, JsonOutputArchive &ar) { SaveN<4>(val.v, ar); }

    void JsonSaveUuid(const Uuid &uuid, JsonOutputArchive &ar) { ar.SaveValue(uuid.ToString()); }
    void JsonLoadUuid(Uuid &uuid, JsonInputArchive &ar) { uuid = Uuid::CreateFromString(ar.LoadString()); }

    void BinarySaveUuid(const Uuid &uuid, BinaryOutputArchive &ar)
    {
        ar.SaveValue(uuid.word[0]);
        ar.SaveValue(uuid.word[1]);
    }
    void BinaryLoadUuid(Uuid &uuid, BinaryInputArchive &ar)
    {
        ar.LoadValue(uuid.word[0]);
        ar.LoadValue(uuid.word[1]);
    }

    void CoreReflection(SerializationContext *context)
    {
        context->Register<Vector2>("Vector2")
            .Member<&Vector2::x>("x")
            .Member<&Vector2::y>("y")
            .JsonSave<&JsonSaveVec2>()
            .JsonLoad<&JsonLoadVec2>();

        context->Register<Vector3>("Vector3")
            .Member<&Vector3::x>("x")
            .Member<&Vector3::y>("y")
            .Member<&Vector3::z>("z")
            .JsonSave<&JsonSaveVec3>()
            .JsonLoad<&JsonLoadVec3>();

        context->Register<Vector4>("Vector4")
            .Member<&Vector4::x>("x")
            .Member<&Vector4::y>("y")
            .Member<&Vector4::z>("z")
            .Member<&Vector4::w>("w")
            .JsonSave<&JsonSaveVec4>()
            .JsonLoad<&JsonLoadVec4>();

        context->Register<Quaternion>("Quaternion")
            .Member<&Quaternion::x>("x")
            .Member<&Quaternion::y>("y")
            .Member<&Quaternion::z>("z")
            .Member<&Quaternion::w>("w")
            .JsonSave<&JsonSaveQuat>()
            .JsonLoad<&JsonLoadQuat>();

        context->Register<Transform>("Transform")
            .Member<&Transform::translation>("translation")
            .Member<&Transform::scale>("scale")
            .Member<&Transform::rotation>("rotation");

        context->Register<Color>("Color")
            .Member<&Color::r>("r")
            .Member<&Color::g>("g")
            .Member<&Color::b>("b")
            .Member<&Color::b>("a");

        context->Register<ColorRGB>("ColorRGB")
            .Member<&ColorRGB::r>("r")
            .Member<&ColorRGB::g>("g")
            .Member<&ColorRGB::b>("b");

        context->Register<Uuid>("Uuid")
            .Member<&Uuid::FromString, &Uuid::ToString>("id")
            .JsonSave<&JsonSaveUuid>()
            .JsonLoad<&JsonLoadUuid>()
            .BinLoad<&BinaryLoadUuid>()
            .BinSave<&BinarySaveUuid>();
    }
} // namespace sky
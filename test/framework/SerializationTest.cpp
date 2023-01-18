//
// Created by Zach Lee on 2021/12/9.
//

#include <core/logger/Logger.h>
#include <framework/serialization/AnyRT.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/serialization/JsonArchive.h>
#include <fstream>
#include <gtest/gtest.h>

using namespace sky;

static const char *TAG = "SerializationTest";

struct TestMember {
    float a;
    float b;
};

struct TestReflect {
    uint32_t        a;
    float           b;
    uint32_t       *c;
    const uint32_t *d;
    const double    e;
    static uint32_t f;
    TestMember      t;
};

uint32_t TestReflect::f = 0;

TEST(SerializationTest, TypeTest)
{
    auto context = SerializationContext::Get();

    context->Register<TestReflect>("TestReflect")
        .Member<&TestReflect::a>("a")
        .Member<&TestReflect::b>("b")
        .Member<&TestReflect::c>("c")
        .Member<&TestReflect::d>("d")
        .Member<&TestReflect::e>("e")
        .Member<&TestReflect::f>("f")
        .Member<&TestReflect::t>("t");

    context->Register<TestMember>("TestMember").Member<&TestMember::a>("a").Member<&TestMember::b>("b");

    auto testReflect = context->FindType("TestReflect");
    ASSERT_NE(testReflect, nullptr);
    ASSERT_EQ(testReflect->members.size(), 7);
    LOG_I(TAG, "name %s", testReflect->info->markedName.data());

    auto testMember = context->FindType("TestMember");
    ASSERT_NE(testMember, nullptr);
    ASSERT_EQ(testMember->members.size(), 2);
    LOG_I(TAG, "name %s", testMember->info->markedName.data());

    for (auto &member : testReflect->members) {
        auto &memberNode = member.second;
        LOG_I(TAG, "name %s, type %s, isFundamental %u, isStatic %u, isConst %u", member.first.data(), memberNode.info->markedName.data(),
              memberNode.info->isFundamental, memberNode.isStatic, memberNode.isConst);
    }

    LOG_I(TAG, "%u, %u, %u, %u", std::is_trivial_v<TestMember>, std::is_trivial_v<TestReflect>, std::is_trivial_v<int>, std::is_trivial_v<int *>);

    Any       v(std::in_place_type<TestReflect>);
    uint32_t *ptr = nullptr;
    ASSERT_EQ(SetAny(v, "a", 5u), true);
    ASSERT_EQ(SetAny(v, "b", 6.f), true);
    ASSERT_EQ(SetAny(v, "c", ptr), true);
    auto val = v.GetAs<TestReflect>();
    ASSERT_EQ(val->a, 5u);
    ASSERT_EQ(val->b, 6.f);
    ASSERT_EQ(val->c, nullptr);

    Any va = GetAny(v, "a");
    ASSERT_EQ(*va.GetAs<uint32_t>(), 5u);
    Any vb = GetAny(v, "b");
    ASSERT_EQ(*vb.GetAs<float>(), 6.f);
    Any vc = GetAny(v, "c");
    ASSERT_EQ(*vc.GetAs<uint32_t *>(), nullptr);
}

struct Ctor1 {
public:
    Ctor1(int va, float vb, double vc, bool vd) : a(va), b(vb), c(vc), d(vd)
    {
    }
    int    a;
    float  b;
    double c;
    bool   d;
};

struct Ctor2 {
    Ctor2(double va, uint64_t vb, int64_t vc, Ctor1 vd) : a(va), b(vb), c(vc), d(vd)
    {
    }
    double   a;
    uint64_t b;
    int64_t  c;
    Ctor1    d;
};

TEST(SerializationTest, ConstructorTest)
{
    auto context = SerializationContext::Get();

    context->Register<Ctor1>("Ctor1")
        .Member<&Ctor1::a>("a")
        .Member<&Ctor1::b>("b")
        .Member<&Ctor1::c>("c")
        .Member<&Ctor1::d>("d")
        .Constructor<int, float, double, bool>();

    context->Register<Ctor2>("Ctor2")
        .Member<&Ctor2::a>("a")
        .Member<&Ctor2::b>("b")
        .Member<&Ctor2::c>("c")
        .Member<&Ctor2::d>("d")
        .Constructor<double, uint64_t, int64_t, Ctor1>();

    {
        Any    any1 = MakeAny<Ctor1>(1, 2.f, 3.0, true);
        Ctor1 *ptr  = any1.GetAs<Ctor1>();
        ASSERT_NE(ptr, nullptr);
        ASSERT_EQ(ptr->a, 1);
        ASSERT_EQ(ptr->b, 2.f);
        ASSERT_EQ(ptr->c, 3.0);
        ASSERT_EQ(ptr->d, true);
    }

    std::string output;
    {
        Any    any2 = MakeAny<Ctor2>(1.0, 3llu, -1ll, Ctor1{1, 2.0f, 3.0, true});
        Ctor2 *ptr  = any2.GetAs<Ctor2>();
        ASSERT_NE(ptr, nullptr);
        ASSERT_EQ(ptr->a, 1);
        ASSERT_EQ(ptr->b, 3llu);
        ASSERT_EQ(ptr->c, -1ll);
        ASSERT_EQ(ptr->d.a, 1);
        ASSERT_EQ(ptr->d.b, 2.0f);
        ASSERT_EQ(ptr->d.c, 3.0);
        ASSERT_EQ(ptr->d.d, true);
    }
}

struct TestObject {
    uint64_t uv1 = 1;
    uint32_t uv2 = 2;
    uint16_t uv3 = 3;
    uint8_t  uv4 = 4;
    int64_t  iv1 = 5;
    int32_t  iv2 = 6;
    int16_t  iv3 = 7;
    int8_t   iv4 = 9;
    float    fv1 = 10;
    double   dv1 = 11;
    bool     bv1 = true;
    bool     bv2 = false;
};

struct TestStruct {
    TestObject t1;
    std::string t2 = "123";
};

TEST(ArchiveTest, JsonArchiveTest)
{
    std::ofstream stream("test.json");
    JsonOutputArchive archive(stream);

    SerializationContext::Get()->Register<TestObject>("TestObject")
        .Member<&TestObject::uv1>("uv1")
        .Member<&TestObject::uv2>("uv2")
        .Member<&TestObject::uv3>("uv3")
        .Member<&TestObject::uv4>("uv4")
        .Member<&TestObject::iv1>("iv1")
        .Member<&TestObject::iv2>("iv2")
        .Member<&TestObject::iv3>("iv3")
        .Member<&TestObject::iv4>("iv4")
        .Member<&TestObject::fv1>("fv1")
        .Member<&TestObject::dv1>("dv1")
        .Member<&TestObject::bv1>("bv1")
        .Member<&TestObject::bv2>("bv2");

    SerializationContext::Get()->Register<TestStruct>("Test")
        .Member<&TestStruct::t1>("t1")
        .Member<&TestStruct::t2>("t2");

    archive.StartObject();
    archive.Key("name");
    archive.SaveValue(1);

    archive.Key("ttt");
    archive.StartArray();
    archive.SaveValue(1);
    archive.SaveValue(2);
    archive.SaveValue(3);
    archive.SaveValue(4);
    archive.EndArray();

    TestStruct obj;

    archive.Key("testObject");
    archive.SaveValue(Any(obj));
    archive.EndObject();
}
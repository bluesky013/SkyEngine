//
// Created by Zach Lee on 2021/12/9.
//

#include <gtest/gtest.h>
#include <core/logger/Logger.h>
#include <framework/serialization/SerializationContext.h>

using namespace sky;

static const char* TAG = "SerializationTest";

struct Test1 {
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;
    uint32_t f;
    uint32_t g;
};

struct Test2 {
    uint64_t a;
    uint64_t b;
    uint64_t c;
    uint64_t d;
};

TEST(SerializationTest, AnyTest)
{
    Any any1(std::in_place_type<Test1>, 1u, 2u, 3u, 4u, 5u, 6u, 7u);
    ASSERT_EQ(&any1, any1.Data());

    Any any2(std::in_place_type<Test2>, 1ull, 2ull, 3ull, 4ull);
    ASSERT_NE(&any2, any2.Data());

    Any any3(std::in_place_type<float>, 5.f);
    ASSERT_EQ(&any3, any3.Data());
}

struct TestMember {
    float a;
    float b;
};

struct TestReflect {
    uint32_t a;
    float b;
    uint32_t* c;
    const uint32_t* d;
    const double e;
    static uint32_t f;
    TestMember t;
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

    context->Register<TestMember>("TestMember")
        .Member<&TestMember::a>("a")
        .Member<&TestMember::b>("b");

    auto testReflect = context->FindType("TestReflect");
    ASSERT_NE(testReflect, nullptr);
    ASSERT_EQ(testReflect->members.size(), 7);
    LOG_I(TAG, "name %s", testReflect->info->typeId.data());

    auto testMember = context->FindType("TestMember");
    ASSERT_NE(testMember, nullptr);
    ASSERT_EQ(testMember->members.size(), 2);
    LOG_I(TAG, "name %s", testMember->info->typeId.data());

    for (auto& member : testReflect->members) {
        auto& memberNode = member.second;
        LOG_I(TAG, "name %s, type %s, isFundamental %u, isStatic %u, isConst %u", member.first.data(),
              memberNode.info->typeId.data(),
              memberNode.info->isFundamental,
              memberNode.isStatic,
              memberNode.isConst);
    }

    LOG_I(TAG, "%u, %u, %u, %u",
         std::is_trivial_v<TestMember>,
         std::is_trivial_v<TestReflect>,
         std::is_trivial_v<int>,
         std::is_trivial_v<int*>);
}
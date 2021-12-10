//
// Created by Zach Lee on 2021/12/9.
//

#include <gtest/gtest.h>
#include <core/logger/Logger.h>
#include <framework/serialization/SerializationContext.h>

using namespace sky;

static const char* TAG = "SerializationTest";

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

class Ctor1 {
public:
    Ctor1() = default;
};

class Ctor2 {
public:
    Ctor2() {}
};

class Ctor3 {
public:
    Ctor3(int) {}
};

class Ctor4 {
    Ctor4() = default;
};

class Ctor5 {
public:
    int a;
};

class Ctor6 {
public:
    int a = 1;
};

class Move1 {
public:
    Move1() = default;
    ~Move1() = default;

    Move1(const Move1&) = default;
    Move1& operator=(const Move1&) = default;
};

class Move2 {
public:
    Move2() = default;
    ~Move2() = default;

    Move2(const Move2&) = default;
    Move2& operator=(const Move2&) = delete;
};

class Move3 {
public:
    Move3() = default;
    ~Move3() = default;

    Move3(const Move3&) = delete;
    Move3& operator=(const Move3&) = delete;
};

class Move4 {
public:
    Move4() = default;
    ~Move4() = default;
};



TEST(SerializationTest, ConstructorTest)
{
    LOG_I(TAG, "default construct, %u, %u, %u, %u, %u, %u",
          std::is_default_constructible_v<Ctor1>,
          std::is_default_constructible_v<Ctor2>,
          std::is_default_constructible_v<Ctor3>,
          std::is_default_constructible_v<Ctor4>,
          std::is_default_constructible_v<Ctor5>,
          std::is_default_constructible_v<Ctor6>);

    LOG_I(TAG, "move construct %u, %u, %u, %u",
          std::is_copy_constructible_v<Move1>,
          std::is_copy_constructible_v<Move2>,
          std::is_copy_constructible_v<Move3>,
          std::is_copy_constructible_v<Move4>);

    std::any any;
}
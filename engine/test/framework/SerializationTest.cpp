//
// Created by Zach Lee on 2021/12/9.
//


#include <core/logger/Logger.h>
#include <core/archive/FileArchive.h>
#include <core/math/Color.h>
#include <framework/serialization/SerializationUtil.h>
#include <framework/serialization/JsonArchive.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/serialization/BinaryArchive.h>
#include <core/type/Container.h>
#include <filesystem>
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

    auto testMember = context->FindType("TestMember");
    ASSERT_NE(testMember, nullptr);
    ASSERT_EQ(testMember->members.size(), 2);

    TestReflect v = {};
    auto typeId = TypeInfo<TestReflect>::RegisteredId();
    uint32_t *ptr = nullptr;
    ASSERT_EQ(SetValue(v, "a", 5u), true);
    ASSERT_EQ(SetValue(v, "b", 6.f), true);
    ASSERT_EQ(SetValue(v, "c", ptr), true);
    ASSERT_EQ(SetValue(v, "d", ptr), true);
    ASSERT_EQ(SetValue(v, "e", 7.0), false);

    ASSERT_EQ(v.a, 5u);
    ASSERT_EQ(v.b, 6.f);
    ASSERT_EQ(v.c, nullptr);

   ASSERT_EQ(*GetValueRaw(reinterpret_cast<void*>(&v), typeId, "a").GetAs<uint32_t>(), 5U);
   ASSERT_EQ(*GetValueRaw(reinterpret_cast<void*>(&v), typeId, "b").GetAs<float>(), 6.f);

   uint32_t **vc = GetValueRaw(reinterpret_cast<void*>(&v), typeId, "c").GetAs<uint32_t*>();
   ASSERT_NE(vc, nullptr);
   ASSERT_EQ(*vc, nullptr);
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
    auto *context = SerializationContext::Get();

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

    {
        Any    any1 = MakeAny(TypeInfo<Ctor1>::RegisteredId(), 1, 2.f, 3.0, true);
        Ctor1 *ptr  = any1.GetAs<Ctor1>();
        ASSERT_NE(ptr, nullptr);
        ASSERT_EQ(ptr->a, 1);
        ASSERT_EQ(ptr->b, 2.f);
        ASSERT_EQ(ptr->c, 3.0);
        ASSERT_EQ(ptr->d, true);
    }

    {
        Any    any1 = MakeAny(TypeInfo<Ctor1>::RegisteredId(), 1, 2.f, 3.0, 4.0);
        ASSERT_EQ(!any1, true);
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

struct GetterSetterTestData {
    int a;
    float b;
    std::string c;
    double d;
};

class GetterSetterTestController {
public:
    explicit GetterSetterTestController(GetterSetterTestData &dt) : data(dt) {}
    ~GetterSetterTestController() = default;

    using MY_CLASS = GetterSetterTestController;
    static void Reflect(SerializationContext* context)
    {
        REGISTER_BEGIN(GetterSetterTestController, context)
            REGISTER_MEMBER(a, SetA, GetA)
            REGISTER_MEMBER(b, SetB, GetB)
            REGISTER_MEMBER(c, SetC, GetC)
            REGISTER_MEMBER(d, SetD, GetD);
    }


    void SetA(const int &v) { data.a = v; }
    const int &GetA() const { return data.a; }

    void SetB(const float &v) { data.b = v; }
    const float &GetB() const { return data.b; }

    void SetC(const std::string &name) { data.c = name; }
    const std::string &GetC() const { return data.c; }

    void SetD(const double &v) { data.d = v; }
    double GetD() const { return data.d; }

private:
    GetterSetterTestData &data;
};

TEST(SerializationTest, GetterSetterTest)
{
    auto *context = SerializationContext::Get();
    GetterSetterTestController::Reflect(context);

    {
        GetterSetterTestData data { 1, 2.f };
        GetterSetterTestController controller(data);

        SetValue(controller, "a", 2);
        SetValue(controller, "b", 3.f);
        SetValue(controller, "c", std::string("test"));
        SetValue(controller, "d", 4.0);
        ASSERT_EQ(data.a, 2);
        ASSERT_EQ(data.b, 3.f);
        ASSERT_EQ(data.c, "test");
        ASSERT_EQ(data.d, 4.0);

        int a = *GetValueConst(controller, "a").GetAsConst<int>();
        float b = *GetValueConst(controller, "b").GetAsConst<float>();

        ASSERT_EQ(a, 2);
        ASSERT_EQ(b, 3.f);

        {
            std::ofstream stream((std::filesystem::temp_directory_path() / "GetterSetterTest.json").string());
            OStreamArchive streamArchive(stream);
            JsonOutputArchive archive(streamArchive);

            archive.SaveValueObject(controller);
        }

        {
            std::ifstream stream((std::filesystem::temp_directory_path() / "GetterSetterTest.json").string());
            IStreamArchive streamArchive(stream);
            JsonInputArchive archive(streamArchive);

            GetterSetterTestData data2 {};
            GetterSetterTestController controller2(data2);

            archive.LoadValueObject(controller2);

            ASSERT_EQ(data2.a, 2);
            ASSERT_EQ(data2.b, 3.f);
            ASSERT_EQ(data2.c, "test");
            ASSERT_EQ(data2.d, 4.0);
        }
    }
}

struct TestContainerVec {
    std::vector<TestMember> vec;
};

struct TestContainerLst {
    std::list<TestMember> lst;
};

TEST(SerializationTest, ContainerVecTest)
{
    TestContainerVec value;
    value.vec.emplace_back(TestMember{1.f, 2.f});
    value.vec.emplace_back(TestMember{3.f, 4.f});

    SerializationContext::Get()->Register<TestContainerVec>("TestContainerVec")
        .Member<&TestContainerVec::vec>("data");

    const auto *info = TypeInfoObj<TestContainerVec>::Get()->RtInfo();
    const auto *typeNode = GetTypeNode(info);

    const auto &member = typeNode->members.at("data");
    auto *view = member.info->containerInfo->sequenceView;

    size_t dataSize = view->Count(&value.vec);
    ASSERT_EQ(dataSize, value.vec.size());

    {
        auto *ptr = view->Emplace(&value.vec);
        SetValue(ptr, TypeInfoObj<TestMember>::Get()->RtInfo()->registeredId, "a", 5.f);
        SetValue(ptr, TypeInfoObj<TestMember>::Get()->RtInfo()->registeredId, "b", 6.f);

        dataSize = view->Count(&value.vec);
        ASSERT_EQ(dataSize, value.vec.size());
        ASSERT_EQ(value.vec[2].a, 5.f);
        ASSERT_EQ(value.vec[2].b, 6.f);
    }

    {
        view->EraseByIndex(&value.vec, 1);
        dataSize = view->Count(&value.vec);
        ASSERT_EQ(dataSize, value.vec.size());
        ASSERT_EQ(value.vec[1].a, 5.f);
        ASSERT_EQ(value.vec[1].b, 6.f);
    }
}

TEST(SerializationTest, ContainerListTest)
{
    TestContainerLst value;
    value.lst.emplace_back(TestMember{1.f, 2.f});
    value.lst.emplace_back(TestMember{3.f, 4.f});

    SerializationContext::Get()->Register<TestContainerLst>("TestContainerLst")
            .Member<&TestContainerLst::lst>("data");

    const auto *info = TypeInfoObj<TestContainerLst>::Get()->RtInfo();
    const auto *typeNode = GetTypeNode(info);

    const auto &member = typeNode->members.at("data");
    auto *view = member.info->containerInfo->sequenceView;

    size_t dataSize = view->Count(&value.lst);
    ASSERT_EQ(dataSize, value.lst.size());

    {
        auto *ptr = view->Emplace(&value.lst);
        SetValue(ptr, TypeInfoObj<TestMember>::Get()->RtInfo()->registeredId, "a", 5.f);
        SetValue(ptr, TypeInfoObj<TestMember>::Get()->RtInfo()->registeredId, "b", 6.f);

        dataSize = view->Count(&value.lst);
        ASSERT_EQ(dataSize, value.lst.size());
        ASSERT_EQ(value.lst.back().a, 5.f);
        ASSERT_EQ(value.lst.back().b, 6.f);
    }

    {
        view->EraseByIndex(&value.lst, 1);
        dataSize = view->Count(&value.lst);
        ASSERT_EQ(dataSize, value.lst.size());

        auto iter = value.lst.begin();
        ASSERT_EQ((*iter).a, 1.f);
        ASSERT_EQ((*iter).b, 2.f);

        ++iter;
        ASSERT_EQ((*iter).a, 5.f);
        ASSERT_EQ((*iter).b, 6.f);
    }
}

class TestMemberFunction {
public:
    TestMemberFunction() = default;
    ~TestMemberFunction() = default;

    uint32_t Foo(uint32_t a) const { return a + 1; } // NOLINT
};

TEST(SerializationTest, MemberFunctionTest)
{
    {
        SerializationContext::Get()->Register<TestMemberFunction>("TestMemberFunction")
            .MemberFunction<&TestMemberFunction::Foo>("Foo");
    }

    {
        auto *type = SerializationContext::Get()->FindType("TestMemberFunction");
        ASSERT_NE(type, nullptr);

        auto fun = type->functions.find("Foo");
        ASSERT_NE(fun, type->functions.end());

        TestMemberFunction tf;

        {
            auto res = InvokeMemberFunctionResult(tf, "Foo", 1u);
            auto *val = res.GetAs<uint32_t>();
            ASSERT_NE(val, nullptr);
            ASSERT_EQ(*val, 2);
        }

        {
            auto res = InvokeMemberFunctionResult(tf, "Foo", 2u);
            auto *val = res.GetAs<uint32_t>();
            ASSERT_NE(val, nullptr);
            ASSERT_EQ(*val, 3);
        }
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

    {
        std::ofstream stream((std::filesystem::temp_directory_path() / "test.json").string());
        OStreamArchive streamArchive(stream);
        JsonOutputArchive archive(streamArchive);

        TestStruct obj;
        obj.t1.uv1 += 10;
        obj.t1.uv2 += 10;
        obj.t1.uv3 += 10;
        obj.t1.uv4 += 10;
        obj.t1.iv1 += 10;
        obj.t1.iv2 += 10;
        obj.t1.iv3 += 10;
        obj.t1.iv4 += 10;
        obj.t1.fv1 += 10;
        obj.t1.dv1 += 10;
        obj.t1.bv1 = false;
        obj.t1.bv2 = true;
        obj.t2 = "test2";

        archive.SaveValueObject(obj);
    }

    {
        std::ifstream stream((std::filesystem::temp_directory_path() / "test.json").string());
        IStreamArchive streamArchive(stream);
        JsonInputArchive archive(streamArchive);

        TestStruct obj;
        archive.LoadValueObject(obj);

        TestStruct dft{};

        ASSERT_EQ(obj.t1.uv1, dft.t1.uv1 += 10);
        ASSERT_EQ(obj.t1.uv2, dft.t1.uv2 += 10);
        ASSERT_EQ(obj.t1.uv3, dft.t1.uv3 += 10);
        ASSERT_EQ(obj.t1.uv4, dft.t1.uv4 += 10);
        ASSERT_EQ(obj.t1.iv1, dft.t1.iv1 += 10);
        ASSERT_EQ(obj.t1.iv2, dft.t1.iv2 += 10);
        ASSERT_EQ(obj.t1.iv3, dft.t1.iv3 += 10);
        ASSERT_EQ(obj.t1.iv4, dft.t1.iv4 += 10);
        ASSERT_EQ(obj.t1.fv1, dft.t1.fv1 += 10);
        ASSERT_EQ(obj.t1.dv1, dft.t1.dv1 += 10);
        ASSERT_EQ(obj.t1.bv1, false);
        ASSERT_EQ(obj.t1.bv2, true);
        ASSERT_EQ(obj.t2, "test2");
    }
}

struct TestSerAE {
    int v1 = 0;
    int v2 = 0;
    int v3 = 0;
};

struct TestSerFunc {
    virtual ~TestSerFunc() = default;

    virtual void Load(JsonInputArchive &archive) = 0;

    virtual void Save(JsonOutputArchive &archive) const = 0;
};

struct TestSerFuncDerv : public TestSerFunc {
    void Load(JsonInputArchive &archive) override {
        archive.Start("a");
        a = archive.LoadInt();
        archive.End();

        archive.Start("b");
        b = static_cast<float>(archive.LoadDouble());
        archive.End();

        archive.StartArray("c");
        for (uint32_t i = 0; i < c.size(); ++i) {
            archive.LoadArrayElement(c[i]);
        }
        archive.End();
    }

    void Save(JsonOutputArchive &archive) const override {
        archive.StartObject();
        archive.Key("a");
        archive.SaveValue(a);
        archive.Key("b");
        archive.SaveValue(b);
        archive.Key("c");
        archive.StartArray();
        for (auto &v : c) {
            archive.SaveValueObject(v);
        }
        archive.EndArray();
        archive.EndObject();
    }

    int a;
    float b;
    std::vector<TestSerAE> c;
};


TEST(ArchiveTest, JsonArchiveRegisterTest)
{

    SerializationContext::Get()
        ->Register<TestSerAE>("TestSerAE")
        .Member<&TestSerAE::v1>("v1")
        .Member<&TestSerAE::v2>("v2")
        .Member<&TestSerAE::v3>("v3");

    SerializationContext::Get()
        ->Register<TestSerFunc>("TestSerFunc")
        .JsonLoad<&TestSerFunc::Load>()
        .JsonSave<&TestSerFunc::Save>();

    SerializationContext::Get()
        ->Register<TestSerFuncDerv>("TestSerFuncDerv")
        .Member<&TestSerFuncDerv::a>("a")
        .Member<&TestSerFuncDerv::b>("b")
        .Member<&TestSerFuncDerv::c>("c")
        .JsonLoad<&TestSerFunc::Load>()
        .JsonSave<&TestSerFunc::Save>();

    {
        TestSerFuncDerv test;
        test.a = 1;
        test.b = 2.f;
        test.c.resize(5, {});
        for (uint32_t i = 0; i < 5; ++i) {
            test.c[i].v1 = i * 3;
            test.c[i].v2 = i * 3 + 1;
            test.c[i].v3 = i * 3 + 2;
        }
        std::ofstream     file((std::filesystem::temp_directory_path() / "json-serialization-test.json").string(), std::ios::binary);
        OStreamArchive streamArchive(file);
        JsonOutputArchive archive(streamArchive);

        TestSerFunc &f = test;
        archive.SaveValueObject(&f, TypeInfo<TestSerFunc>::RegisteredId());
    }

    {
        TestSerFuncDerv  test;
        std::ifstream    file((std::filesystem::temp_directory_path() / "json-serialization-test.json").string(), std::ios::binary);
        IStreamArchive streamArchive(file);
        JsonInputArchive archive(streamArchive);

        TestSerFunc &f = test;
        archive.LoadValueById(&f, TypeInfo<TestSerFunc>::RegisteredId());
    }
}

TEST(ArchiveTest, BinaryArchiveRegister_FundamentalTest)
{
    {
        OFileArchive file((std::filesystem::temp_directory_path() / "binary-fundamental-test.bin").string(), std::ios::binary);
        BinaryOutputArchive archive(file);
        archive.SaveValue(-1);
        archive.SaveValue(2U);
        archive.SaveValue(3.F);
        archive.SaveValue(std::string("abcd"));
        archive.SaveValue(5LLU);
        archive.SaveValue(true);
    }

    {
        IFileArchive file((std::filesystem::temp_directory_path() / "binary-fundamental-test.bin").string(), std::ios::binary);
        BinaryInputArchive archive(file);
        {
            int value = 0;
            archive.LoadValue(value);
            ASSERT_EQ(value, -1);
        }
        {
            uint32_t value = 0;
            archive.LoadValue(value);
            ASSERT_EQ(value, 2U);
        }
        {
            float value = 0;
            archive.LoadValue(value);
            ASSERT_EQ(value, 3.F);
        }
        {
            std::string value;
            archive.LoadValue(value);
            ASSERT_EQ(value, std::string("abcd"));
        }
        {
            uint64_t value = 0;
            archive.LoadValue(value);
            ASSERT_EQ(value, 5LLU);
        }
        {
            bool value = 0;
            archive.LoadValue(value);
            ASSERT_EQ(value, true);
        }
    }
}

struct TestBinArchive_B1 {
    int a = 0;
    float b = 0.f;
};

struct TestBinArchive_B2 {
    uint32_t c = 0;
    double d = 0.0;
};

struct TestBinArchive {
    TestBinArchive_B1 b1;
    TestBinArchive_B2 b2;
};

TEST(ArchiveTest, BinaryArchiveRegister_ClassTest)
{
    auto *context = SerializationContext::Get();

    context->Register<TestBinArchive_B1>("TestBinArchive_B1")
        .Member<&TestBinArchive_B1::a>("a")
        .Member<&TestBinArchive_B1::b>("b");

    context->Register<TestBinArchive_B2>("TestBinArchive_B2")
        .Member<&TestBinArchive_B2::c>("c")
        .Member<&TestBinArchive_B2::d>("d");

    context->Register<TestBinArchive>("TestBinArchive")
        .Member<&TestBinArchive::b1>("b1")
        .Member<&TestBinArchive::b2>("b2");

    {
        TestBinArchive test = {{1, 2.f}, {3, 4.0}};
        OFileArchive file((std::filesystem::temp_directory_path() / "binary-class-test.bin").string(), std::ios::binary);
        BinaryOutputArchive archive(file);
        archive.SaveObject(&test, TypeInfo<TestBinArchive>::RegisteredId());
    }

    {
        IFileArchive file((std::filesystem::temp_directory_path() / "binary-class-test.bin").string(), std::ios::binary);
        BinaryInputArchive archive(file);

        TestBinArchive test = {};
        archive.LoadObject(&test, TypeInfo<TestBinArchive>::RegisteredId());

        ASSERT_EQ(test.b1.a, 1);
        ASSERT_EQ(test.b1.b, 2.f);
        ASSERT_EQ(test.b2.c, 3);
        ASSERT_EQ(test.b2.d, 4.0);
    }
}

TEST(SerializationTest, ColorAlphaReflectionTest)
{
    const auto *node = GetTypeNode(TypeInfo<Color>::RegisteredId());
    ASSERT_NE(node, nullptr);
    ASSERT_EQ(node->members.count("a"), 1);

    const auto *member = GetTypeMember("a", TypeInfo<Color>::RegisteredId());
    ASSERT_NE(member, nullptr);
    ASSERT_NE(member->setterFn, nullptr);
    ASSERT_NE(member->getterConstFn, nullptr);

    Color color(0.f, 0.f, 0.f, 0.f);
    float alpha = 0.5f;
    ASSERT_TRUE(member->setterFn(&color, &alpha));
    ASSERT_FLOAT_EQ(color.a, 0.5f);
    ASSERT_FLOAT_EQ(color.b, 0.f);

    const auto any = member->getterConstFn(&color);
    const auto *read = any.GetAsConst<float>();
    ASSERT_NE(read, nullptr);
    ASSERT_FLOAT_EQ(*read, 0.5f);
}

struct AnyLargeValue {
    double v[8] = {0, 0, 0, 0, 0, 0, 0, 0};
};

struct AnySmallMoveOnly {
    int32_t v = 0;

    AnySmallMoveOnly() = default;
    explicit AnySmallMoveOnly(int32_t x) : v(x) {}
    AnySmallMoveOnly(const AnySmallMoveOnly &) = delete;
    AnySmallMoveOnly &operator=(const AnySmallMoveOnly &) = delete;
    AnySmallMoveOnly(AnySmallMoveOnly &&) noexcept = default;
    AnySmallMoveOnly &operator=(AnySmallMoveOnly &&) noexcept = default;
};

TEST(SerializationTest, AnyValueSemanticsTest)
{
    AnyLargeValue large{};
    for (int i = 0; i < 8; ++i) {
        large.v[i] = static_cast<double>(i) + 0.5;
    }

    Any a(std::in_place_type<AnyLargeValue>, large);

    Any b;
    b = a;
    ASSERT_NE(b.GetAs<AnyLargeValue>(), nullptr);
    ASSERT_DOUBLE_EQ(b.GetAs<AnyLargeValue>()->v[3], 3.5);

    Any c(std::in_place_type<AnyLargeValue>, large);
    c = a;
    ASSERT_DOUBLE_EQ(c.GetAs<AnyLargeValue>()->v[7], 7.5);

    a = a;
    ASSERT_DOUBLE_EQ(a.GetAs<AnyLargeValue>()->v[1], 1.5);

    Any d = std::move(c);
    ASSERT_NE(d.GetAs<AnyLargeValue>(), nullptr);
    ASSERT_DOUBLE_EQ(d.GetAs<AnyLargeValue>()->v[0], 0.5);

    Any e(std::in_place_type<AnyLargeValue>, large);
    e = std::move(d);
    ASSERT_DOUBLE_EQ(e.GetAs<AnyLargeValue>()->v[6], 6.5);

    Any s(std::in_place_type<AnySmallMoveOnly>, 7);
    Any t = std::move(s);
    ASSERT_NE(t.GetAs<AnySmallMoveOnly>(), nullptr);
    ASSERT_EQ(t.GetAs<AnySmallMoveOnly>()->v, 7);
}

enum class BinTestEnum : uint32_t { A = 1, B = 2, C = 5 };

struct BinTestEnumHolder {
    BinTestEnum e = BinTestEnum::A;
};

struct BinSeqElem {
    uint32_t a = 0;
    float    b = 0.f;
};

struct BinSeqVec {
    std::vector<BinSeqElem> items;
};

struct BinSeqLst {
    std::list<BinSeqElem> items;
};

TEST(ArchiveTest, BinaryArchiveReflection_EnumTest)
{
    auto *context = SerializationContext::Get();
    context->Register<BinTestEnum>("BinTestEnum")
        .Enum(BinTestEnum::A, "A")
        .Enum(BinTestEnum::B, "B")
        .Enum(BinTestEnum::C, "C");
    context->Register<BinTestEnumHolder>("BinTestEnumHolder")
        .Member<&BinTestEnumHolder::e>("e");

    const auto path = std::filesystem::temp_directory_path() / "binary-enum-test.bin";
    {
        BinTestEnumHolder holder;
        holder.e = BinTestEnum::C;
        OFileArchive file(path.string(), std::ios::binary);
        BinaryOutputArchive archive(file);
        archive.SaveObject(&holder, TypeInfo<BinTestEnumHolder>::RegisteredId());
    }
    {
        IFileArchive file(path.string(), std::ios::binary);
        BinaryInputArchive archive(file);
        BinTestEnumHolder holder;
        archive.LoadObject(&holder, TypeInfo<BinTestEnumHolder>::RegisteredId());
        ASSERT_EQ(holder.e, BinTestEnum::C);
    }
}

TEST(ArchiveTest, BinaryArchiveReflection_SequenceTest)
{
    auto *context = SerializationContext::Get();
    context->Register<BinSeqElem>("BinSeqElem")
        .Member<&BinSeqElem::a>("a")
        .Member<&BinSeqElem::b>("b");
    context->Register<BinSeqVec>("BinSeqVec")
        .Member<&BinSeqVec::items>("items");
    context->Register<BinSeqLst>("BinSeqLst")
        .Member<&BinSeqLst::items>("items");

    {
        const auto path = std::filesystem::temp_directory_path() / "binary-seq-vec-test.bin";
        BinSeqVec value;
        value.items.push_back(BinSeqElem{1, 1.5f});
        value.items.push_back(BinSeqElem{2, 2.5f});
        OFileArchive file(path.string(), std::ios::binary);
        BinaryOutputArchive archive(file);
        archive.SaveObject(&value, TypeInfo<BinSeqVec>::RegisteredId());
    }
    {
        const auto path = std::filesystem::temp_directory_path() / "binary-seq-vec-test.bin";
        IFileArchive file(path.string(), std::ios::binary);
        BinaryInputArchive archive(file);
        BinSeqVec value;
        archive.LoadObject(&value, TypeInfo<BinSeqVec>::RegisteredId());
        ASSERT_EQ(value.items.size(), 2u);
        ASSERT_EQ(value.items[0].a, 1u);
        ASSERT_FLOAT_EQ(value.items[0].b, 1.5f);
        ASSERT_EQ(value.items[1].a, 2u);
        ASSERT_FLOAT_EQ(value.items[1].b, 2.5f);
    }
    {
        const auto path = std::filesystem::temp_directory_path() / "binary-seq-lst-test.bin";
        BinSeqLst value;
        value.items.push_back(BinSeqElem{3, 3.5f});
        value.items.push_back(BinSeqElem{4, 4.5f});
        OFileArchive file(path.string(), std::ios::binary);
        BinaryOutputArchive archive(file);
        archive.SaveObject(&value, TypeInfo<BinSeqLst>::RegisteredId());
    }
    {
        const auto path = std::filesystem::temp_directory_path() / "binary-seq-lst-test.bin";
        IFileArchive file(path.string(), std::ios::binary);
        BinaryInputArchive archive(file);
        BinSeqLst value;
        archive.LoadObject(&value, TypeInfo<BinSeqLst>::RegisteredId());
        ASSERT_EQ(value.items.size(), 2u);
        auto iter = value.items.begin();
        ASSERT_EQ(iter->a, 3u);
        ASSERT_FLOAT_EQ(iter->b, 3.5f);
        ++iter;
        ASSERT_EQ(iter->a, 4u);
        ASSERT_FLOAT_EQ(iter->b, 4.5f);
    }
}

class TestMemberFunctionArgs {
public:
    uint32_t Foo(uint32_t a) const { return a + 1; } // NOLINT
};

TEST(ArchiveTest, MemberFunctionArgMismatchTest)
{
    SerializationContext::Get()->Register<TestMemberFunctionArgs>("TestMemberFunctionArgs")
        .MemberFunction<&TestMemberFunctionArgs::Foo>("Foo");

    TestMemberFunctionArgs obj;

    {
        auto res = InvokeMemberFunctionResult(obj, "Foo", 41u);
        auto *val = res.GetAs<uint32_t>();
        ASSERT_NE(val, nullptr);
        ASSERT_EQ(*val, 42u);
    }
    {
        auto res = InvokeMemberFunctionResult(obj, "Foo", std::string("bad"));
        ASSERT_FALSE(static_cast<bool>(res));
    }
    {
        auto res = InvokeMemberFunctionResult(obj, "Foo");
        ASSERT_FALSE(static_cast<bool>(res));
    }
}

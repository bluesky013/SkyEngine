//
// Created by Zach Lee on 2021/12/9.
//


#include <core/logger/Logger.h>
#include <core/archive/FileArchive.h>
#include <framework/serialization/SerializationUtil.h>
#include <framework/serialization/JsonArchive.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/serialization/BinaryArchive.h>
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

   ASSERT_EQ(*GetValue(reinterpret_cast<void*>(&v), typeId, "a").GetAs<uint32_t>(), 5U);
   ASSERT_EQ(*GetValue(reinterpret_cast<void*>(&v), typeId, "b").GetAs<float>(), 6.f);

   uint32_t **vc = GetValue(reinterpret_cast<void*>(&v), typeId, "c").GetAs<uint32_t*>();
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
            std::ofstream stream("GetterSetterTest.json");
            JsonOutputArchive archive(stream);

            archive.SaveValueObject(controller);
        }

        {
            std::ifstream stream("GetterSetterTest.json");
            JsonInputArchive archive(stream);

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
        std::ofstream stream("test.json");
        JsonOutputArchive archive(stream);

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
        std::ifstream stream("test.json");
        JsonInputArchive archive(stream);

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
        std::ofstream     file("json-serialization-test.json", std::ios::binary);
        JsonOutputArchive archive(file);

        TestSerFunc &f = test;
        archive.SaveValueObject(&f, TypeInfo<TestSerFunc>::RegisteredId());
    }

    {
        TestSerFuncDerv  test;
        std::ifstream    file("json-serialization-test.json", std::ios::binary);
        JsonInputArchive archive(file);

        TestSerFunc &f = test;
        archive.LoadValueById(&f, TypeInfo<TestSerFunc>::RegisteredId());
    }
}

TEST(ArchiveTest, BinaryArchiveRegister_FundamentalTest)
{
    {
        OFileArchive file("binary-fundamental-test.bin", std::ios::binary);
        BinaryOutputArchive archive(file);
        archive.SaveValue(-1);
        archive.SaveValue(2U);
        archive.SaveValue(3.F);
        archive.SaveValue(std::string("abcd"));
        archive.SaveValue(5LLU);
        archive.SaveValue(true);
    }

    {
        IFileArchive file("binary-fundamental-test.bin", std::ios::binary);
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
        OFileArchive file("binary-class-test.bin", std::ios::binary);
        BinaryOutputArchive archive(file);
        archive.SaveObject(&test, TypeInfo<TestBinArchive>::RegisteredId());
    }

    {
        IFileArchive file("binary-class-test.bin", std::ios::binary);
        BinaryInputArchive archive(file);

        TestBinArchive test = {};
        archive.LoadObject(&test, TypeInfo<TestBinArchive>::RegisteredId());

        ASSERT_EQ(test.b1.a, 1);
        ASSERT_EQ(test.b1.b, 2.f);
        ASSERT_EQ(test.b2.c, 3);
        ASSERT_EQ(test.b2.d, 4.0);
    }
}

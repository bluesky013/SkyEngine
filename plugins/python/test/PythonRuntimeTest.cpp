//
// Created by blues on 2024/6/2.
//

#include <gtest/gtest.h>

#include <core/environment/Environment.h>
#include <framework/serialization/SerializationContext.h>
#include <python/PythonApi.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace sky;
using namespace sky::py;

enum class BindColor : uint32_t { Red = 1, Green = 2, Blue = 3 };

struct BindObject {
    float x = 0.f;
    BindColor color = BindColor::Red;
    std::vector<int32_t> items;
    int32_t Add(int32_t value) const { return static_cast<int32_t>(x) + value; } // NOLINT
};

static void RegisterBindTestTypes()
{
    auto *context = SerializationContext::Get();
    context->Register<BindColor>("BindColor")
        .Enum(BindColor::Red, "Red")
        .Enum(BindColor::Green, "Green")
        .Enum(BindColor::Blue, "Blue");
    context->Register<BindObject>("BindObject")
        .Member<&BindObject::x>("x")
        .Member<&BindObject::color>("color")
        .Member<&BindObject::items>("items")
        .MemberFunction<&BindObject::Add>("Add");
}

int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc, argv);
    PythonAttachEnvironment(Environment::Get());
    RegisterBindTestTypes();
    return RUN_ALL_TESTS();
}

TEST(PythonRuntimeTest, InitRequiresEnvironment)
{
    PythonDetachEnvironment();
    EXPECT_FALSE(PythonInit());
    PythonAttachEnvironment(Environment::Get());
    EXPECT_TRUE(PythonInit());
    PythonShutdown();
}

TEST(PythonRuntimeTest, InitRunShutdown)
{
    if (!PythonInit()) {
        GTEST_SKIP() << "python interpreter unavailable";
    }

    EXPECT_TRUE(PythonRunString("value = 40 + 2"));
    EXPECT_FALSE(PythonRunString("def broken(:"));

    PythonShutdown();
}

TEST(PythonRuntimeTest, ReflectionBindings)
{
    if (!PythonInit()) {
        GTEST_SKIP() << "python interpreter unavailable";
    }

    EXPECT_TRUE(PythonRunString(
        "import sky\n"
        "assert 'Color' in sky.types()\n"
        "c = sky.make('Color')\n"
        "c.r = 0.25\n"
        "assert abs(c.r - 0.25) < 1e-6\n"
        "assert abs(c.g) < 1e-6\n"));

    EXPECT_TRUE(PythonRunString(
        "import sky\n"
        "t = sky.type('Color')\n"
        "assert t is not None\n"));

    EXPECT_FALSE(PythonRunString("import sky\nsky.make('NoSuchType')"));
    EXPECT_FALSE(PythonRunString("import sky\nc = sky.make('Color')\nc.no_such_member"));

    PythonShutdown();
}

TEST(PythonRuntimeTest, EnumBindings)
{
    if (!PythonInit()) {
        GTEST_SKIP() << "python interpreter unavailable";
    }

    EXPECT_TRUE(PythonRunString(
        "import sky\n"
        "o = sky.make('BindObject')\n"
        "assert o.color == 1\n"
        "o.color = 2\n"
        "assert o.color == 2\n"
        "enum_type = sky.type('BindColor')\n"
        "assert enum_type.Blue == 3\n"));

    PythonShutdown();
}

TEST(PythonRuntimeTest, MemberFunctionBindings)
{
    if (!PythonInit()) {
        GTEST_SKIP() << "python interpreter unavailable";
    }

    EXPECT_TRUE(PythonRunString(
        "import sky\n"
        "o = sky.make('BindObject')\n"
        "o.x = 3.0\n"
        "r = o.Add(4)\n"
        "assert r == 7\n"));

    EXPECT_FALSE(PythonRunString(
        "import sky\n"
        "o = sky.make('BindObject')\n"
        "o.Add('bad')\n"));

    PythonShutdown();
}

TEST(PythonRuntimeTest, SequenceBindings)
{
    if (!PythonInit()) {
        GTEST_SKIP() << "python interpreter unavailable";
    }

    EXPECT_TRUE(PythonRunString(
        "import sky\n"
        "o = sky.make('BindObject')\n"
        "assert len(o.items) == 0\n"
        "o.items.append(10)\n"
        "o.items.append(20)\n"
        "assert len(o.items) == 2\n"
        "assert o.items[0] == 10\n"
        "assert o.items[1] == 20\n"
        "assert list(o.items) == [10, 20]\n"
        "o.items.erase(0)\n"
        "assert len(o.items) == 1\n"
        "assert o.items[0] == 20\n"));

    PythonShutdown();
}

TEST(PythonRuntimeTest, EmbeddedTextScript)
{
    if (!PythonInit()) {
        GTEST_SKIP() << "python interpreter unavailable";
    }

    const std::string script =
        "import json, math, re, zlib, collections\n"
        "data = {'items': [1, 2, 3, 4], 'name': 'sky'}\n"
        "assert sum(data['items']) == 10\n"
        "assert json.loads(json.dumps(data)) == data\n"
        "assert abs(math.sqrt(16.0) - 4.0) < 1e-9\n"
        "assert re.match(r'^sk', data['name']) is not None\n"
        "assert collections.Counter(data['items'])[2] == 1\n"
        "assert len(zlib.compress(b'skyengine')) > 0\n"
        "assert [x * x for x in range(5)] == [0, 1, 4, 9, 16]\n";

    EXPECT_TRUE(PythonRunString(script.data()));

    PythonShutdown();
}

TEST(PythonRuntimeTest, Tier1Builtins)
{
    if (!PythonInit()) {
        GTEST_SKIP() << "python interpreter unavailable";
    }

    EXPECT_TRUE(PythonRunString(
        "import unicodedata, _decimal, _uuid, _zoneinfo, _elementtree, pyexpat, _bz2, _lzma\n"
        "import _socket, select, _overlapped, _queue\n"
        "import socket, decimal, zlib, json, re, bz2, lzma, xml.etree.ElementTree\n"
        "print('tier1 builtins ok')\n"));

    PythonShutdown();
}

TEST(PythonRuntimeTest, ReferenceStabilitySmoke)
{
    if (!PythonInit()) {
        GTEST_SKIP() << "python interpreter unavailable";
    }

    EXPECT_TRUE(PythonRunString(
        "import sky\n"
        "for _ in range(2000):\n"
        "    o = sky.make('BindObject')\n"
        "    o.x = 1.5\n"
        "    o.items.append(1)\n"
        "    o.items.erase(0)\n"
        "    assert o.Add(2) == 3\n"));

    PythonShutdown();
}

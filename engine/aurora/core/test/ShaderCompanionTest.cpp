//
// ShaderCompanion tests: `.slang.json` schema parsing + resolution.
//

#include <aurora/shader/ShaderCompanion.h>
#include <aurora/shader/ShaderFileSystem.h>

#include <gtest/gtest.h>

using namespace sky;
using namespace sky::aurora;

namespace {

    const char *kCompanion = R"({
        "sources": [
            {"name": "material", "width": 8},
            {"name": "vertex",   "width": 8}
        ],
        "variants": [
            {"key": "USE_SHADOWS", "source": "material", "width": 1, "default": 0},
            {"key": "NUM_LIGHTS",  "source": "material", "width": 4, "spec": 7}
        ],
        "vertex": [
            {"name": "HAS_VERTEX_COLOR", "semantics": ["COLOR"]}
        ],
        "entries": ["VSMain", "FSMain"],
        "depends": ["common.slang"]
    })";

} // namespace

TEST(ShaderCompanionTest, Parse)
{
    ShaderCompanion c;
    std::string     error;
    ASSERT_TRUE(ParseShaderCompanion(kCompanion, c, &error)) << error;

    ASSERT_EQ(c.schema.sources.size(), 2u);
    EXPECT_EQ(c.schema.sources[0].name, Name("material"));
    EXPECT_EQ(c.schema.sources[0].bitOffset, 0);
    EXPECT_EQ(c.schema.sources[1].bitOffset, 8);
    EXPECT_EQ(c.schema.totalBits, 16u);

    ASSERT_EQ(c.schema.entries.size(), 2u);
    EXPECT_EQ(c.schema.entries[0].key, Name("USE_SHADOWS"));
    EXPECT_EQ(c.schema.entries[0].bitOffset, 0);
    EXPECT_EQ(c.schema.entries[0].bitWidth, 1);
    EXPECT_FALSE(c.schema.entries[0].isSpec);
    EXPECT_EQ(c.schema.entries[1].bitOffset, 1);
    EXPECT_TRUE(c.schema.entries[1].isSpec);
    EXPECT_EQ(c.schema.entries[1].specId, 7u);

    ASSERT_EQ(c.vertexDefs.size(), 1u);
    EXPECT_EQ(c.vertexDefs[0].name, Name("HAS_VERTEX_COLOR"));
    ASSERT_EQ(c.vertexDefs[0].semantics.size(), 1u);
    EXPECT_EQ(c.vertexDefs[0].semantics[0], VertexSemantic::COLOR);

    EXPECT_EQ(c.entryPoints.size(), 2u);
    EXPECT_EQ(c.depends.size(), 1u);
    EXPECT_NE(c.fingerprint, 0u);
}

TEST(ShaderCompanionTest, LoadViaFileSystem)
{
    ShaderFileSystem fs;
    fs.AddVirtualFile("material/lit.slang.json", kCompanion);

    ShaderCompanion c;
    std::string     error;
    ASSERT_TRUE(LoadShaderCompanion(fs, "material/lit.slang", c, &error)) << error;
    EXPECT_EQ(c.schema.totalBits, 16u);

    ShaderCompanion missing;
    EXPECT_FALSE(LoadShaderCompanion(fs, "material/missing.slang", missing, &error));
}

TEST(ShaderCompanionTest, InvalidRejected)
{
    ShaderCompanion c;
    std::string     error;
    EXPECT_FALSE(ParseShaderCompanion("{ not json", c, &error));

    const char *badSource = R"({
        "sources": [{"name": "material", "width": 8}],
        "variants": [{"key": "X", "source": "nope", "width": 1}]
    })";
    EXPECT_FALSE(ParseShaderCompanion(badSource, c, &error));
}

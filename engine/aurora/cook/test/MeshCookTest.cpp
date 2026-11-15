//
// Mesh cook pipeline tests: source import, assembly, optimization,
// asset writer round-trips, config presets.
//

#include <aurora/cook/mesh/MeshAssembler.h>
#include <aurora/cook/mesh/MeshAssetWriter.h>
#include <aurora/cook/mesh/MeshBuildConfig.h>
#include <aurora/cook/mesh/MeshOptimizer.h>
#include <aurora/cook/mesh/MeshSource.h>

#include <aurora/rhi/Core.h>
#include <aurora/rhi/VertexSemantic.h>
#include <core/archive/MemoryStreamArchive.h>
#include <framework/serialization/BinaryArchive.h>
#include <framework/serialization/JsonArchive.h>

#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::cook;

namespace {

    const char *kQuadObj = R"(
v 0 0 0
v 1 0 0
v 1 1 0
v 0 1 0
vt 0 0
vt 1 0
vt 1 1
vt 0 1
vn 0 0 1
usemtl testmat
f 1/1/1 2/2/1 3/3/1
f 1/1/1 3/3/1 4/4/1
)";

    std::vector<uint8_t> ToBytes(const char *str)
    {
        return {str, str + std::strlen(str)};
    }

    CookedMesh CookQuad(const MeshBuildConfig &config = {})
    {
        CookMeshSource source;
        EXPECT_TRUE(LoadMeshSource(ToBytes(kQuadObj), ".obj", source));

        CookedMesh             mesh;
        MeshAssembler::Payload assemble;
        assemble.source = &source;
        assemble.config = &config;
        assemble.out    = &mesh;
        MeshAssembler(assemble).DoWork();
        return mesh;
    }

    template <typename T>
    void RoundTrip(const T &in, T &out)
    {
        OMemoryArchive      outArchive;
        BinaryOutputArchive writer(outArchive);
        in.Save(writer);

        IMemoryArchive     inArchive(outArchive.Data(), outArchive.Size());
        BinaryInputArchive reader(inArchive);
        out.Load(reader);
    }

} // namespace

TEST(MeshCookSourceTest, ParseObjQuad)
{
    CookMeshSource source;
    ASSERT_TRUE(LoadMeshSource(ToBytes(kQuadObj), ".obj", source));

    ASSERT_EQ(source.primitives.size(), 1u);
    const auto &prim = source.primitives[0];
    // assimp expands face-corner vertex triplets (pos/uv/normal); the quad
    // yields 6 vertices for 2 triangles
    EXPECT_EQ(prim.positions.size(), 6u);
    EXPECT_EQ(prim.indices.size(), 6u);
    EXPECT_FALSE(prim.normals.empty());
    EXPECT_FALSE(prim.uvs.empty());
    EXPECT_FALSE(source.Skinned());
    ASSERT_EQ(source.materialNames.size(), 1u);
    EXPECT_EQ(source.materialNames[0], "testmat");
    EXPECT_EQ(prim.materialIndex, 0u);
}

TEST(MeshCookSourceTest, RejectGarbage)
{
    CookMeshSource source;
    EXPECT_FALSE(LoadMeshSource(ToBytes("not a mesh"), ".obj", source));
}

TEST(MeshCookAssembleTest, InterleavedStreamLayout)
{
    const CookedMesh mesh = CookQuad();

    EXPECT_EQ(mesh.vertexCount, 6u);
    EXPECT_EQ(mesh.indexCount, 6u);
    EXPECT_EQ(mesh.indexType, static_cast<uint32_t>(IndexType::U16));
    ASSERT_EQ(mesh.subMeshes.size(), 1u);
    EXPECT_EQ(mesh.subMeshes[0].indexOffset, 0u);
    EXPECT_EQ(mesh.subMeshes[0].indexCount, 6u);

    // CalcTangentSpace runs on sources with normals+uvs: POSITION 12 +
    // NORMAL 12 + TANGENT 16 + UV1 8 = 48
    bool hasTangent = false;
    for (const auto &attr : mesh.attributes) {
        if (attr.semantic == static_cast<uint8_t>(VertexSemantic::TANGENT)) {
            hasTangent = true;
        }
    }
    EXPECT_TRUE(hasTangent);
    EXPECT_EQ(mesh.vertexStride, 48u);
    EXPECT_EQ(mesh.vertexData.size(), static_cast<size_t>(mesh.vertexStride) * mesh.vertexCount);
    EXPECT_EQ(mesh.indexData.size(), static_cast<size_t>(mesh.indexCount) * 2);

    // attributes are tightly packed in declaration order
    uint32_t offset = 0;
    for (const auto &attr : mesh.attributes) {
        EXPECT_EQ(attr.offset, offset);
        offset = attr.offset + 0; // checked via stride accumulation below
        switch (static_cast<VertexSemantic>(attr.semantic)) {
        case VertexSemantic::POSITION:
        case VertexSemantic::NORMAL: offset += 12; break;
        case VertexSemantic::TANGENT:
        case VertexSemantic::WEIGHTS:
        case VertexSemantic::COLOR: offset += 16; break;
        case VertexSemantic::UV1: offset += 8; break;
        case VertexSemantic::JOINTS: offset += 4; break;
        default: break;
        }
    }
    EXPECT_EQ(offset, mesh.vertexStride);

    // bounds of the quad
    EXPECT_FLOAT_EQ(mesh.bounds.min.x, 0.f);
    EXPECT_FLOAT_EQ(mesh.bounds.min.y, 0.f);
    EXPECT_FLOAT_EQ(mesh.bounds.max.x, 1.f);
    EXPECT_FLOAT_EQ(mesh.bounds.max.y, 1.f);
}

TEST(MeshCookAssembleTest, TangentsCanBeDisabled)
{
    MeshBuildConfig config;
    config.tangents       = false;
    const CookedMesh mesh = CookQuad(config);
    for (const auto &attr : mesh.attributes) {
        EXPECT_NE(attr.semantic, static_cast<uint8_t>(VertexSemantic::TANGENT));
    }
}

TEST(MeshCookOptimizeTest, PreservesCountsAndRanges)
{
    CookedMesh             mesh = CookQuad();
    MeshOptimizer::Payload optimize;
    optimize.mesh = &mesh;
    MeshOptimizer(optimize).DoWork();

    EXPECT_EQ(mesh.indexCount, 6u);
    EXPECT_EQ(mesh.vertexCount, 6u);
    ASSERT_EQ(mesh.subMeshes.size(), 1u);
    EXPECT_EQ(mesh.subMeshes[0].indexCount, 6u);
    EXPECT_EQ(mesh.indexData.size(), static_cast<size_t>(mesh.indexCount) * 2);
    EXPECT_EQ(mesh.vertexData.size(), static_cast<size_t>(mesh.vertexStride) * mesh.vertexCount);
}

TEST(MeshCookWriterTest, MeshAssetRoundTrip)
{
    CookedMesh mesh = CookQuad();

    Uuid          skeleton; // no skeleton
    MeshAssetData data;
    WriteMeshAsset(mesh, data);
    EXPECT_EQ(data.version, MeshAssetData::CURRENT_VERSION);
    EXPECT_FALSE(data.HasSkinInputs());
    EXPECT_EQ(data.vertexSize, mesh.vertexData.size());
    EXPECT_EQ(data.indexSize, mesh.indexData.size());

    MeshAssetData loaded;
    RoundTrip(data, loaded);
    EXPECT_EQ(loaded.vertexStride, data.vertexStride);
    EXPECT_EQ(loaded.vertexCount, data.vertexCount);
    EXPECT_EQ(loaded.indexCount, data.indexCount);
    EXPECT_EQ(loaded.indexType, data.indexType);
    ASSERT_EQ(loaded.attributes.size(), data.attributes.size());
    EXPECT_EQ(loaded.vertexOffset, data.vertexOffset);
    EXPECT_EQ(loaded.vertexSize, data.vertexSize);
    EXPECT_EQ(loaded.indexOffset, data.indexOffset);
    EXPECT_EQ(loaded.indexSize, data.indexSize);
    ASSERT_EQ(loaded.subMeshes.size(), data.subMeshes.size());
    EXPECT_FLOAT_EQ(loaded.bounds.center.x, data.bounds.center.x);
    EXPECT_FLOAT_EQ(loaded.bounds.extent.y, data.bounds.extent.y);
}

TEST(MeshCookWriterTest, VersionMismatchRejected)
{
    MeshAssetData data;
    CookedMesh    mesh = CookQuad();
    WriteMeshAsset(mesh, data);
    data.version = 1; // v1 payloads must be rejected by the current loader

    MeshAssetData loaded;
    RoundTrip(data, loaded);
    EXPECT_EQ(loaded.vertexSize, 0u);
    EXPECT_TRUE(loaded.attributes.empty());
}

TEST(MeshCookConfigTest, LoadJsonAndResolve)
{
    const char *json =
        R"({"defaultBundle":"mesh_pc","bundles":{"common":{"tangents":true,"optimize":true},"mesh_pc":{"tangents":true,"optimize":true,"defaultMaterial":"12345678-1234-1234-1234-123456789ABC"}}})";
    MeshBuildPresets presets;
    ASSERT_TRUE(presets.LoadJson(json));

    ASSERT_EQ(presets.bundles.size(), 2u);

    std::string            key;
    const MeshBuildConfig *cfg = presets.Resolve("mesh_pc", key);
    ASSERT_NE(cfg, nullptr);
    EXPECT_EQ(key, "mesh_pc");
    EXPECT_TRUE(cfg->optimize);
    EXPECT_EQ(cfg->defaultMaterial, Uuid::CreateFromString("12345678-1234-1234-1234-123456789ABC"));

    // unknown key falls back to the default bundle
    cfg = presets.Resolve("does_not_exist", key);
    ASSERT_NE(cfg, nullptr);
    EXPECT_EQ(key, "mesh_pc");
}

TEST(MeshCookConfigTest, InvalidDefaultMaterialPreservesConfiguration)
{
    MeshBuildPresets presets;
    presets.defaultBundle = "previous";
    presets.bundles.clear();
    presets.bundles["previous"] = {};
    for (const auto *json : {R"({"bundles":{"common":{"defaultMaterial":17}}})", R"({"bundles":{"common":{"defaultMaterial":"invalid"}}})",
                             R"({"bundles":{"common":{"defaultMaterial":"12345678X1234-1234-1234-123456789ABC"}}})",
                             R"({"bundles":{"common":{"defaultMaterial":"X2345678-1234-1234-1234-123456789ABC"}}})",
                             R"({"bundles":{"common":{"defaultMaterial":"00000000-0000-0000-0000-000000000000"}}})",
                             R"({"bundles":{"common":{"optimize":17}}})", R"({"bundles":null})", "[]", "invalid"}) {
        EXPECT_FALSE(presets.LoadJson(json));
        EXPECT_EQ(presets.defaultBundle, "previous");
        ASSERT_EQ(presets.bundles.size(), 1u);
        EXPECT_TRUE(presets.bundles.contains("previous"));
    }
}

TEST(MeshCookWriterTest, MaterialBindingHasNoImplicitFallbackAndPreservesValidOutput)
{
    Asset<Mesh> asset;
    const auto  material = Uuid::Create();
    asset.Data().subMeshes.resize(1);
    asset.Data().subMeshes.front().materialIndex = 2;
    asset.Data().materials                       = {material};
    EXPECT_FALSE(BindMeshMaterials(asset, {}));
    EXPECT_EQ(asset.Data().materials, std::vector<Uuid>{material});
    const std::vector<Uuid> supplied(3, material);
    ASSERT_TRUE(BindMeshMaterials(asset, supplied));
    EXPECT_EQ(asset.Data().materials, supplied);
    asset.Data().subMeshes.front().materialIndex = 8192;
    EXPECT_FALSE(BindMeshMaterials(asset, supplied));
    EXPECT_EQ(asset.Data().materials, supplied);
}

TEST(MeshCookConfigTest, CommonOverridesDefaultsAndReloadKeepsOnlyDeclaredBundles)
{
    MeshBuildPresets presets;
    ASSERT_TRUE(presets.LoadJson(
        R"({"bundles":{"common":{"tangents":false,"optimize":false,"skinning":false,"defaultMaterial":"12345678-1234-1234-1234-123456789ABC"}}})"));
    ASSERT_EQ(presets.bundles.size(), 1u);
    std::string key;
    const auto *config = presets.Resolve("common", key);
    ASSERT_NE(config, nullptr);
    EXPECT_FALSE(config->tangents);
    EXPECT_FALSE(config->optimize);
    EXPECT_FALSE(config->skinning);
    EXPECT_EQ(config->defaultMaterial, Uuid::CreateFromString("12345678-1234-1234-1234-123456789ABC"));
    ASSERT_TRUE(presets.LoadJson(R"({"defaultBundle":"custom","bundles":{"custom":{"optimize":false}}})"));
    ASSERT_EQ(presets.bundles.size(), 1u);
    EXPECT_FALSE(presets.bundles.contains("common"));
    config = presets.Resolve("common", key);
    ASSERT_NE(config, nullptr);
    EXPECT_EQ(key, "custom");
    EXPECT_FALSE(config->optimize);
    EXPECT_FALSE(config->defaultMaterial);
}

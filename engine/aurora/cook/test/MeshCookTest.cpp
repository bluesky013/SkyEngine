//
// Mesh cook pipeline tests: source import, assembly, optimization, meshlets,
// asset writer round-trips, config presets.
//

#include <aurora/cook/mesh/MeshAssetWriter.h>
#include <aurora/cook/mesh/MeshAssembler.h>
#include <aurora/cook/mesh/MeshBuildConfig.h>
#include <aurora/cook/mesh/MeshOptimizer.h>
#include <aurora/cook/mesh/MeshletBuilder.h>
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

        CookedMesh mesh;
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
        OMemoryArchive     outArchive;
        BinaryOutputArchive writer(outArchive);
        in.Save(writer);

        IMemoryArchive    inArchive(outArchive.Data(), outArchive.Size());
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
        case VertexSemantic::NORMAL:  offset += 12; break;
        case VertexSemantic::TANGENT:
        case VertexSemantic::WEIGHTS:
        case VertexSemantic::COLOR:   offset += 16; break;
        case VertexSemantic::UV1:     offset += 8;  break;
        case VertexSemantic::JOINTS:  offset += 4;  break;
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
    config.tangents = false;
    const CookedMesh mesh = CookQuad(config);
    for (const auto &attr : mesh.attributes) {
        EXPECT_NE(attr.semantic, static_cast<uint8_t>(VertexSemantic::TANGENT));
    }
}

TEST(MeshCookOptimizeTest, PreservesCountsAndRanges)
{
    CookedMesh mesh = CookQuad();
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

TEST(MeshCookMeshletTest, BuildsMeshletsWithBounds)
{
    CookedMesh mesh = CookQuad();
    MeshletBuilder::Payload meshlets;
    meshlets.mesh = &mesh;
    MeshletBuilder(meshlets).DoWork();

    ASSERT_EQ(mesh.meshlets.size(), 1u); // a quad fits in one meshlet
    EXPECT_EQ(mesh.meshlets[0].vertexCount, 6u);
    EXPECT_EQ(mesh.meshlets[0].triangleCount, 2u);
    EXPECT_EQ(mesh.meshletBounds.size(), 1u);
    EXPECT_EQ(mesh.meshletVertices.size(), 6u);
    EXPECT_EQ(mesh.meshletTriangles.size(), 6u);
    EXPECT_GT(mesh.meshletBounds[0].radius, 0.f);
}

TEST(MeshCookWriterTest, MeshAssetRoundTrip)
{
    MeshBuildConfig config;
    config.meshlets = true;
    CookedMesh mesh = CookQuad(config);
    MeshletBuilder::Payload meshlets;
    meshlets.mesh = &mesh;
    MeshletBuilder(meshlets).DoWork();

    Uuid skin; // no skin
    MeshAssetData data;
    WriteMeshAsset(mesh, skin, data);
    EXPECT_EQ(data.version, MeshAssetData::CURRENT_VERSION);
    EXPECT_TRUE(data.HasMeshlets());
    EXPECT_FALSE(data.HasSkin());

    MeshAssetData loaded;
    RoundTrip(data, loaded);
    EXPECT_EQ(loaded.vertexStride, data.vertexStride);
    EXPECT_EQ(loaded.vertexCount, data.vertexCount);
    EXPECT_EQ(loaded.indexCount, data.indexCount);
    EXPECT_EQ(loaded.indexType, data.indexType);
    ASSERT_EQ(loaded.attributes.size(), data.attributes.size());
    EXPECT_EQ(loaded.vertexData, data.vertexData);
    EXPECT_EQ(loaded.indexData, data.indexData);
    ASSERT_EQ(loaded.subMeshes.size(), data.subMeshes.size());
    EXPECT_EQ(loaded.meshlets.size(), data.meshlets.size());
    EXPECT_EQ(loaded.meshletVertices, data.meshletVertices);
    EXPECT_EQ(loaded.meshletTriangles, data.meshletTriangles);
    ASSERT_EQ(loaded.meshletBounds.size(), data.meshletBounds.size());
    EXPECT_FLOAT_EQ(loaded.bounds.min.x, data.bounds.min.x);
    EXPECT_FLOAT_EQ(loaded.bounds.max.y, data.bounds.max.y);
}

TEST(MeshCookWriterTest, VersionMismatchRejected)
{
    MeshAssetData data;
    CookedMesh    mesh = CookQuad();
    WriteMeshAsset(mesh, Uuid{}, data);
    data.version = 1; // v1 payloads must be rejected by the v2 loader

    MeshAssetData loaded;
    RoundTrip(data, loaded);
    EXPECT_TRUE(loaded.vertexData.empty());
    EXPECT_TRUE(loaded.attributes.empty());
}

TEST(MeshCookWriterTest, SkinAssetRoundTrip)
{
    SkinAssetData skin;
    skin.inverseBindMatrices = {Matrix4::Identity(), Matrix4::Identity()};
    skin.boneNames           = {"root", "spine"};
    skin.boneMapping         = {0, 1};

    SkinAssetData out;
    WriteSkinAsset(skin, out);
    EXPECT_EQ(out.version, SkinAssetData::CURRENT_VERSION);

    SkinAssetData loaded;
    RoundTrip(out, loaded);
    ASSERT_EQ(loaded.inverseBindMatrices.size(), 2u);
    ASSERT_EQ(loaded.boneNames.size(), 2u);
    EXPECT_EQ(loaded.boneNames[1], "spine");
    ASSERT_EQ(loaded.boneMapping.size(), 2u);
    EXPECT_EQ(loaded.boneMapping[1], 1u);
}

TEST(MeshCookConfigTest, LoadJsonAndResolve)
{
    const char *json = R"({"defaultBundle":"mesh_pc","bundles":{"common":{"tangents":true,"optimize":true,"meshlets":false},"mesh_pc":{"tangents":true,"optimize":true,"meshlets":true}}})";
    std::vector<uint8_t> bytes(json, json + std::strlen(json));

    IMemoryArchive   in(bytes.data(), bytes.size());
    JsonInputArchive archive(in);

    MeshBuildPresets presets;
    presets.LoadJson(archive);

    ASSERT_EQ(presets.bundles.size(), 2u);

    std::string key;
    const MeshBuildConfig *cfg = presets.Resolve("mesh_pc", key);
    ASSERT_NE(cfg, nullptr);
    EXPECT_EQ(key, "mesh_pc");
    EXPECT_TRUE(cfg->meshlets);
    EXPECT_TRUE(cfg->optimize);

    // unknown key falls back to the default bundle
    cfg = presets.Resolve("does_not_exist", key);
    ASSERT_NE(cfg, nullptr);
    EXPECT_EQ(key, "mesh_pc");
}

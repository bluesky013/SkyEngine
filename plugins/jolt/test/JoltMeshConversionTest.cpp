#include "JoltMeshConversion.h"
#include "JoltBackend.h"

#include <aurora/cook/mesh/MeshAssembler.h>
#include <aurora/cook/mesh/MeshAssetWriter.h>
#include <aurora/cook/mesh/MeshBuildConfig.h>
#include <aurora/cook/mesh/MeshSource.h>
#include <aurora/rhi/VertexSemantic.h>
#include <core/archive/MemoryStreamArchive.h>
#include <cstring>
#include <gtest/gtest.h>

using namespace sky;
using namespace sky::phy;

namespace {
    class CollisionProduct final : public Asset<aurora::Mesh> {
    public:
        void PublishBulk()
        {
            AddBulk(Data().vertexData.data(), Data().vertexData.size());
            AddBulk(Data().indexData.data(), Data().indexData.size());
            OMemoryArchive      header;
            BinaryOutputArchive output(header);
            Data().Save(output);
            IMemoryArchive     saved(header.Data(), header.Size());
            BinaryInputArchive input(saved);
            Data().Load(input);
            Data().vertexData.clear();
            Data().indexData.clear();
            productStream = new IMemoryArchive(bulk.data(), bulk.size());
        }
    };
    void CookQuad(Asset<aurora::Mesh> &asset)
    {
        using namespace aurora::cook;
        const std::string obj =
            "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nvn 0 0 1\nf 1/1/1 2/2/1 3/3/1\nf 1/1/1 3/3/1 4/4/1\n";
        CookMeshSource source;
        ASSERT_TRUE(LoadMeshSource(std::vector<uint8_t>(obj.begin(), obj.end()), ".obj", source));
        MeshBuildConfig config;
        CookedMesh      cooked;
        ASSERT_TRUE(MeshAssembler({&source, &config, &cooked}).DoWork());
        WriteMeshAsset(cooked, asset.Data());
    }
} // namespace

TEST(JoltMeshConversionTest, CookedInterleavedBulkSupportsBothIndexWidths)
{
    for (bool wide : {false, true}) {
        CollisionProduct asset;
        CookQuad(asset);
        auto &data = asset.Data();
        ASSERT_GT(data.vertexStride, sizeof(Vector3));
        ASSERT_EQ(data.indexType, static_cast<uint32_t>(aurora::IndexType::U16));
        if (wide) {
            std::vector<uint8_t> indices(data.indexCount * sizeof(uint32_t));
            for (uint32_t i = 0; i < data.indexCount; ++i) {
                uint16_t shortIndex;
                std::memcpy(&shortIndex, data.indexData.data() + i * sizeof(uint16_t), sizeof(uint16_t));
                const uint32_t index = shortIndex;
                std::memcpy(indices.data() + i * sizeof(uint32_t), &index, sizeof(uint32_t));
            }
            data.indexData = std::move(indices);
            data.indexType = static_cast<uint32_t>(aurora::IndexType::U32);
            data.indexSize = data.indexData.size();
        }
        asset.PublishBulk();
        ASSERT_TRUE(asset.Data().vertexData.empty());
        std::string error;
        auto        mesh = ConvertAuroraCollisionMesh(asset, error);
        ASSERT_TRUE(mesh) << error;
        EXPECT_EQ(mesh->indexType, wide ? IndexType::U32 : IndexType::U16);
        JoltBackend backend;
        ASSERT_TRUE(backend.Init());
        std::unique_ptr<IPhysicsWorld> world(backend.CreateWorld({}));
        ASSERT_TRUE(world);
        PhysicsBodyDesc body;
        body.shape.type     = ShapeType::TriangleMesh;
        body.shape.meshData = mesh;
        ASSERT_TRUE(IsValid(world->CreateBody(body))) << world->GetLastStatus().message;
        PhysicsQueryHit hit;
        EXPECT_TRUE(world->Raycast({.25f, .25f, 2.f}, {0, 0, -1}, 5, {}, hit));
        EXPECT_NEAR(hit.position.z, 0.f, .001f);
    }
}

TEST(JoltMeshConversionTest, PositionOffsetAndMalformedProductsAreChecked)
{
    Asset<aurora::Mesh> asset;
    CookQuad(asset);
    auto &data     = asset.Data();
    auto  position = std::find_if(data.attributes.begin(), data.attributes.end(),
                                  [](const auto &attribute) { return attribute.semantic == static_cast<uint8_t>(aurora::VertexSemantic::POSITION); });
    ASSERT_NE(position, data.attributes.end());
    const auto oldOffset = position->offset;
    const auto offset    = data.vertexStride - sizeof(Vector3);
    for (uint32_t i = 0; i < data.vertexCount; ++i) {
        Vector3 vertex;
        std::memcpy(&vertex, data.vertexData.data() + i * data.vertexStride + oldOffset, sizeof(vertex));
        std::memcpy(data.vertexData.data() + i * data.vertexStride + offset, &vertex, sizeof(vertex));
    }
    position->offset = static_cast<uint32_t>(offset);
    std::string error;
    ASSERT_TRUE(ConvertAuroraCollisionMesh(asset, error)) << error;
    position->offset = data.vertexStride;
    EXPECT_FALSE(ConvertAuroraCollisionMesh(asset, error));
    position->offset  = static_cast<uint32_t>(offset);
    data.indexData[0] = data.indexData[1] = 0xff;
    EXPECT_FALSE(ConvertAuroraCollisionMesh(asset, error));
    data.indexData.pop_back();
    EXPECT_FALSE(ConvertAuroraCollisionMesh(asset, error));
}

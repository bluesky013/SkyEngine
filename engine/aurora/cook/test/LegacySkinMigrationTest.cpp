#include <aurora/adaptor/AuroraReflection.h>
#include <aurora/adaptor/assets/SkinAsset.h>
#include <aurora/cook/mesh/AuroraMeshBuilder.h>
#include <aurora/cook/mesh/MeshAssembler.h>
#include <aurora/cook/mesh/MeshAssetWriter.h>
#include <aurora/cook/mesh/MeshSource.h>
#include <core/archive/MemoryStreamArchive.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetManager.h>
#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::cook;

namespace {
    bool WriteLegacyMesh(const FilePtr &file, const Uuid &rig, const CookMeshSource &source, const CookedMesh &mesh, bool truncated)
    {
        OMemoryArchive      bytes;
        BinaryOutputArchive header(bytes);
        header.SaveValue(uint32_t(5));
        header.SaveValue(uint64_t(0));
        header.SaveValue(uint64_t(mesh.vertexData.size()));
        header.SaveValue(mesh.vertexStride);
        header.SaveValue(mesh.vertexCount);
        header.SaveValue(uint32_t(mesh.attributes.size()));
        for (const auto &attribute : mesh.attributes) {
            header.SaveValue(attribute.semantic);
            header.SaveValue(attribute.semanticIndex);
            header.SaveValue(attribute.format);
            header.SaveValue(attribute.offset);
        }
        header.SaveValue(uint64_t(mesh.vertexData.size()));
        header.SaveValue(uint64_t(mesh.indexData.size()));
        header.SaveValue(mesh.indexCount);
        header.SaveValue(mesh.indexType);
        header.SaveValue(uint64_t(mesh.vertexData.size() + mesh.indexData.size()));
        header.SaveValue(uint64_t(source.skin.binds.size() * sizeof(Matrix4)));
        header.SaveValue(uint32_t(source.skin.binds.size()));
        header.SaveValue(uint32_t(0));
        header.SaveValue(uint32_t(source.skin.binds.size()));
        for (const auto &bind : source.skin.binds)
            header.SaveValue(bind.targetBone);
        header.SaveValue(rig);
        header.SaveValue(uint32_t(mesh.subMeshes.size()));
        for (const auto &sub : mesh.subMeshes) {
            header.SaveValue(sub.indexOffset);
            header.SaveValue(sub.indexCount);
            header.SaveValue(sub.materialIndex);
        }
        header.SaveValue(uint32_t(0));
        const BoundingBoxSphere bounds(mesh.bounds);
        header.SaveValue(bounds.center);
        header.SaveValue(bounds.extent);
        header.SaveValue(bounds.radius);
        const auto output = file->WriteAsArchive();
        const auto size   = uint32_t(truncated ? 8 : bytes.Size());
        if (!output || !output->Save(std::string("AuroraMesh")) || !output->Save(uint32_t(1)) || !output->Save(rig.word[0]) ||
            !output->Save(rig.word[1]) || !output->Save(std::string{}) || !output->Save(size) || !output->SaveRaw(bytes.Data(), size) ||
            !output->Save(uint64_t(mesh.vertexData.size() + mesh.indexData.size() + source.skin.binds.size() * sizeof(Matrix4))) ||
            !output->SaveRaw(reinterpret_cast<const char *>(mesh.vertexData.data()), mesh.vertexData.size()) ||
            !output->SaveRaw(reinterpret_cast<const char *>(mesh.indexData.data()), mesh.indexData.size()))
            return false;
        for (const auto &bind : source.skin.binds)
            if (!output->SaveRaw(reinterpret_cast<const char *>(&bind.inverseBind), sizeof(Matrix4)))
                return false;
        output->Flush();
        return bool(output->GetStream());
    }
} // namespace

TEST(LegacySkinMigration, ExactUniqueRigKeepsIdentityAndInvalidEvidenceNeverReusesIt)
{
    const auto                  fixture = std::filesystem::path(__FILE__).parent_path() / "fixtures/four-influences.gltf";
    std::ifstream               input(fixture, std::ios::binary);
    const std::vector<uint8_t>  bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    std::vector<CookMeshSource> sources;
    ASSERT_TRUE(LoadMeshSources(bytes, ".gltf", sources));
    ASSERT_EQ(sources.size(), 1u);
    MeshBuildConfig config;
    CookedMesh      mesh;
    ASSERT_TRUE(MeshAssembler({&sources[0], &config, &mesh}).DoWork());
    auto *manager = AssetManager::Get();
    manager->RegisterAssetHandler<Mesh>();
    manager->RegisterAssetHandler<Skin>();
    manager->RegisterAssetHandler<Skeleton>();
    auto *database = AssetDataBase::Get();
    for (int mode = 0; mode < 4; ++mode) {
        const auto          root = std::filesystem::temp_directory_path() / ("sky-legacy-skin-" + Uuid::Create().ToString());
        NativeFileSystemPtr fs   = new NativeFileSystem(FilePath(root));
        ASSERT_TRUE(fs->WriteFileExclusive("character.gltf", bytes));
        database->Reset();
        database->SetWorkSpaceFs(fs);
        const auto source = database->RegisterAsset("character.gltf", false);
        ASSERT_TRUE(source);
        const auto          bundleName = Uuid::Create().ToString();
        AssetProductBundle *bundle     = new HashedAssetBundle(fs, bundleName);
        manager->AddAssetProductBundle(bundle);
        const auto rigId      = Uuid::Create();
        auto       rigProduct = std::static_pointer_cast<Asset<Skeleton>>(manager->CreateBuildAsset(rigId, Name("AuroraSkeleton")));
        ASSERT_TRUE(rigProduct);
        WriteSkeletonAsset(sources[0], rigProduct->Data());
        if (mode == 1)
            rigProduct->Data().refPos[0].translation.x += 1.f;
        if (mode != 2)
            manager->SaveAsset(rigProduct, bundleName);
        ASSERT_TRUE(WriteLegacyMesh(bundle->CreateOrOpenFile(source->uuid), rigId, sources[0], mesh, mode == 3));
        AssetProductInspection previous;
        ASSERT_TRUE(manager->InspectProduct(source->uuid, bundleName, previous));
        EXPECT_EQ(previous.dependencies, std::vector<Uuid>{rigId});
        EXPECT_FALSE(manager->FindAsset(source->uuid));
        AuroraMeshBuilder builder;
        const std::string presets = R"({"defaultBundle":"migration","bundles":{"migration":{}}})";
        ASSERT_TRUE(fs->WriteFileExclusive("mesh_build_presets.json", {reinterpret_cast<const uint8_t *>(presets.data()), presets.size()}));
        builder.LoadConfig(fs);
        AssetBuildRequest request;
        request.assetInfo = source;
        request.file      = fs->OpenFile("character.gltf");
        request.bundle    = bundleName;
        AssetBuildResult result{};
        builder.Request(request, result);
        ASSERT_EQ(result.retCode, AssetBuildRetCode::SUCCESS) << result.error;
        std::map<std::string, Uuid> defaults;
        ASSERT_TRUE(database->GetGeneratedDefaults(source->uuid, defaults));
        EXPECT_EQ(defaults.at("mesh"), source->uuid);
        EXPECT_EQ(defaults.at("skeleton") == rigId, mode == 0);
        const auto first = defaults;
        builder.Request(request, result);
        ASSERT_EQ(result.retCode, AssetBuildRetCode::SUCCESS) << result.error;
        ASSERT_TRUE(database->GetGeneratedDefaults(source->uuid, defaults));
        EXPECT_EQ(defaults, first);
        uint32_t version = 0;
        ASSERT_TRUE(previous.payload->Load(version));
        EXPECT_EQ(version, 5u);
        AssetProductInspection published;
        ASSERT_TRUE(manager->InspectProduct(source->uuid, bundleName, published));
        EXPECT_TRUE(published.dependencies.empty());
        EXPECT_TRUE(published.group);
        EXPECT_FALSE(manager->FindAsset(source->uuid));
        database->Reset();
        database->SetWorkSpaceFs({});
        fs.Reset(nullptr);
        std::error_code error;
        std::filesystem::remove_all(root, error);
        EXPECT_FALSE(error);
    }
}

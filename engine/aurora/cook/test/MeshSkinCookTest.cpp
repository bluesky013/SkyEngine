#include <aurora/adaptor/AuroraReflection.h>
#include <aurora/adaptor/assets/SkinAsset.h>
#include <aurora/cook/mesh/AuroraMeshBuilder.h>
#include <aurora/cook/mesh/MeshAssembler.h>
#include <aurora/cook/mesh/MeshAssetWriter.h>
#include <aurora/cook/mesh/MeshOptimizer.h>
#include <aurora/cook/mesh/MeshSource.h>
#include <aurora/cook/skeleton/AuroraSkeletonBuilder.h>
#include <aurora/rhi/VertexSemantic.h>
#include <core/archive/MemoryStreamArchive.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetProduct.h>
#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::cook;

namespace {
    std::vector<uint8_t> Fixture(const std::string &name = "two-bindings")
    {
        const auto    path = std::filesystem::path(__FILE__).parent_path() / "fixtures" / (name + ".gltf");
        std::ifstream input(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }
    template <class T>
    T Read(const CookedMesh &mesh, VertexSemantic semantic, uint32_t vertex)
    {
        T value{};
        for (const auto &attribute : mesh.attributes) {
            if (attribute.semantic == uint8_t(semantic)) {
                std::memcpy(&value, mesh.vertexData.data() + size_t(vertex) * mesh.vertexStride + attribute.offset, sizeof(T));
            }
        }
        return value;
    }
    void NearMatrix(const Matrix4 &a, const Matrix4 &b)
    {
        for (size_t i = 0; i < 16; ++i) {
            EXPECT_NEAR(a.v[i], b.v[i], 1e-4f);
        }
    }
    class MeshProductFiles : public NativeFileSystem {
    public:
        using NativeFileSystem::NativeFileSystem;
        bool                fail = false;
        FileOperationStatus ReplaceFileAtomic(const FilePath &from, const FilePath &to) const override
        {
            return fail && to.FileName().ends_with(".bin") ? FileOperationStatus{FileOperationError::IOError, "injected mesh commit failure"}
                                                           : NativeFileSystem::ReplaceFileAtomic(from, to);
        }
    };
} // namespace

TEST(MeshSkinCook, OriginalFifthInfluenceIsRejectedAndFourAreNormalized)
{
    std::vector<CookMeshSource> sources;
    const auto                  rejected = LoadMeshSources(Fixture("five-influences"), ".gltf", sources);
    EXPECT_EQ(rejected.error, MeshCookError::Unsupported) << rejected.message;
    EXPECT_TRUE(sources.empty());
    const auto valid = LoadMeshSources(Fixture("four-influences"), ".gltf", sources);
    ASSERT_TRUE(valid) << valid.message;
    ASSERT_EQ(sources.size(), 1U);
    EXPECT_EQ(sources[0].bones.size(), 5U);
    for (size_t i = 0; i < 4; ++i) {
        EXPECT_FLOAT_EQ(sources[0].primitives[0].weights[0][i], .25f);
    }
}

TEST(MeshSkinCook, BadReferenceShearSingularTransformAndZeroWeightFail)
{
    for (const auto *name : {"reference", "shear", "singular", "zero", "negative"}) {
        std::vector<CookMeshSource> sources;
        const auto                  result = LoadMeshSources(Fixture(name), ".gltf", sources);
        EXPECT_FALSE(result) << name;
        EXPECT_TRUE(sources.empty()) << name;
    }
}

TEST(MeshSkinCook, TwoBindingsPreserveDistinctInverseBindsAndHelperClosure)
{
    std::vector<CookMeshSource> sources;
    const auto                  status = LoadMeshSources(Fixture(), ".gltf", sources);
    ASSERT_TRUE(status) << status.message;
    ASSERT_EQ(sources.size(), 2U);
    ASSERT_EQ(sources[0].skin.binds.size(), 2U);
    ASSERT_EQ(sources[1].skin.binds.size(), 2U);
    EXPECT_EQ(sources[0].skin.input, sources[1].skin.input);
    EXPECT_NE(sources[0].skin.binds[0].inverseBind[3][1], sources[1].skin.binds[0].inverseBind[3][1]);
    for (const auto &source : sources) {
        ASSERT_EQ(source.bones.size(), 4U);
        EXPECT_NE(source.bones[2].name.find("Helper"), std::string::npos);
        std::vector<Matrix4> models;
        for (size_t i = 0; i < source.bones.size(); ++i) {
            const auto &bone = source.bones[i];
            EXPECT_TRUE(bone.parentIndex == 0xFFFF || bone.parentIndex < i);
            models.push_back(bone.parentIndex == 0xFFFF ? bone.refTransform.ToMatrix() : models[bone.parentIndex] * bone.refTransform.ToMatrix());
        }
        for (const auto &bind : source.skin.binds) {
            for (size_t i = 0; i < source.bones.size(); ++i) {
                if (source.bones[i].name == bind.targetBone) {
                    NearMatrix(models[i] * bind.inverseBind, Matrix4::Identity());
                }
            }
        }
        MeshBuildConfig config;
        CookedMesh      mesh;
        ASSERT_TRUE(MeshAssembler({&source, &config, &mesh}).DoWork());
        ASSERT_TRUE(MeshOptimizer({&mesh}).DoWork());
        ASSERT_TRUE(ValidateCookedMesh(mesh));
        for (uint32_t vertex = 0; vertex < mesh.vertexCount; ++vertex) {
            const auto point   = Read<Vector3>(mesh, VertexSemantic::POSITION, vertex);
            const auto normal  = Read<Vector3>(mesh, VertexSemantic::NORMAL, vertex);
            const auto tangent = Read<Vector4>(mesh, VertexSemantic::TANGENT, vertex);
            EXPECT_NEAR(normal.Length(), 1.f, 1e-5f);
            EXPECT_NEAR(normal.x * tangent.x + normal.y * tangent.y + normal.z * tangent.z, 0.f, 1e-5f);
            EXPECT_GE(point.x, 2.f);
            EXPECT_LE(point.x, 4.f);
            for (const auto &bounds : mesh.slotBounds) {
                EXPECT_TRUE(bounds.occupied);
                EXPECT_LE(bounds.bounds.min.x, point.x);
                EXPECT_GE(bounds.bounds.max.x, point.x);
            }
        }
        MeshAssetData asset;
        WriteMeshAsset(mesh, asset);
        EXPECT_TRUE(asset.Validate()) << asset.Validate().message;
        OMemoryArchive      bytes;
        BinaryOutputArchive output(bytes);
        asset.Save(output);
        IMemoryArchive     input(bytes.Data(), bytes.Size());
        BinaryInputArchive archive(input);
        MeshAssetData      loaded;
        loaded.Load(archive);
        ASSERT_TRUE(loaded.IsLoadValid()) << loaded.validation.message;
        EXPECT_EQ(loaded.skinInput, source.skin.input);
        EXPECT_EQ(loaded.slotBounds.size(), 2U);
    }
}

TEST(MeshSkinCook, LimitsSlot255AndPostOptimizeValidation)
{
    std::vector<CookMeshSource> sources;
    ASSERT_TRUE(LoadMeshSources(Fixture("four-influences"), ".gltf", sources));
    auto &source = sources[0];
    while (source.bones.size() < 256) {
        source.bones.push_back({"extra/" + std::to_string(source.bones.size()), 0, {}});
    }
    while (source.skin.binds.size() < 256) {
        source.skin.binds.push_back(source.skin.binds[0]);
        source.skin.input.slots.push_back("slot/" + std::to_string(source.skin.input.slots.size()));
    }
    for (size_t i = 0; i < source.primitives[0].positions.size(); ++i) {
        source.primitives[0].joints[i]  = {255, 0, 0, 0};
        source.primitives[0].weights[i] = {1, 0, 0, 0};
    }
    MeshBuildConfig config;
    CookedMesh      mesh;
    ASSERT_TRUE(MeshAssembler({&source, &config, &mesh}).DoWork());
    ASSERT_TRUE(MeshOptimizer({&mesh}).DoWork());
    EXPECT_TRUE(mesh.slotBounds[255].occupied);
    EXPECT_FALSE(mesh.slotBounds[0].occupied);
    source.bones.push_back({"over-limit", 0, {}});
    EXPECT_FALSE(ValidateMeshSource(source));
    source.bones.pop_back();
    source.skin.binds.push_back(source.skin.binds[0]);
    source.skin.input.slots.push_back("slot/256");
    EXPECT_FALSE(ValidateMeshSource(source));
    auto broken = mesh;
    for (const auto &attribute : broken.attributes) {
        if (attribute.semantic == uint8_t(VertexSemantic::WEIGHTS)) {
            const float zero = 0;
            std::memcpy(broken.vertexData.data() + attribute.offset, &zero, sizeof(zero));
        }
    }
    EXPECT_FALSE(ValidateCookedMesh(broken));
    broken                              = mesh;
    broken.slotBounds[255].bounds.max.x = -100;
    EXPECT_FALSE(ValidateCookedMesh(broken));
}

TEST(MeshSkinCook, HelperMotionAndActorTransformAgreeWithCpuLbsBounds)
{
    std::vector<CookMeshSource> sources;
    ASSERT_TRUE(LoadMeshSources(Fixture(), ".gltf", sources));
    const auto       &source = sources[0];
    SkeletonAssetData rigData;
    WriteSkeletonAsset(source, rigData);
    const auto rig = Skeleton::BuildSkeleton(rigData);
    ASSERT_NE(rig, nullptr);
    SkinTargetLayout target;
    ASSERT_TRUE(MakeSkinTargetLayout(*rig, target));
    const auto     skin = Skin::Create(source.skin);
    SkinBindingKey key{Uuid::Create(), Uuid::Create(), Uuid::Create(), 1, 1, 1, target.signature, Uuid::Create(), Uuid::Create(), Uuid::Create()};
    ResolvedSkinBindingPtr binding;
    ASSERT_TRUE(ResolveSkinBinding(source.skin.input, *skin, target, key, binding));
    auto movedModels = target.referenceModels;
    movedModels[2][3][0] += 2;
    movedModels[3][3][0] += 2;
    std::vector<Matrix4> palette, reference;
    ASSERT_TRUE(BuildSkinningPalette(*binding, movedModels, palette));
    ASSERT_TRUE(BuildSkinningPalette(*binding, target.referenceModels, reference));
    for (const auto &matrix : reference) {
        NearMatrix(matrix, Matrix4::Identity());
    }
    MeshBuildConfig config;
    CookedMesh      mesh;
    ASSERT_TRUE(MeshAssembler({&source, &config, &mesh}).DoWork());
    Transform actor;
    actor.translation = {5, 2, 3};
    actor.scale       = {2, 1, .5f};
    actor.rotation    = Quaternion(.4f, Vector3{0, 0, 1});
    const auto world  = actor.ToMatrix();
    AABB       bounds;
    bool       first = true;
    for (size_t slot = 0; slot < mesh.slotBounds.size(); ++slot) {
        if (!mesh.slotBounds[slot].occupied) {
            continue;
        }
        const auto transformed = AABB::Transform(mesh.slotBounds[slot].bounds, world * palette[slot]);
        if (first) {
            bounds = transformed;
            first  = false;
        } else {
            bounds.Merge(transformed);
        }
    }
    for (uint32_t vertex = 0; vertex < mesh.vertexCount; ++vertex) {
        const auto    point   = Read<Vector3>(mesh, VertexSemantic::POSITION, vertex);
        const auto    weights = Read<Vector4>(mesh, VertexSemantic::WEIGHTS, vertex);
        const auto    joints  = Read<std::array<uint8_t, 4>>(mesh, VertexSemantic::JOINTS, vertex);
        const Vector4 homogeneous{point.x, point.y, point.z, 1};
        Vector4       deformed{};
        for (uint32_t i = 0; i < 4; ++i) {
            deformed += (palette[joints[i]] * homogeneous) * weights[i];
        }
        const auto actual   = world * deformed;
        const auto expected = world * Vector4{point.x + 1.5f, point.y, point.z, 1};
        for (uint32_t axis = 0; axis < 3; ++axis) {
            EXPECT_NEAR(actual[axis], expected[axis], 1e-4f);
            EXPECT_GE(actual[axis], bounds.min[axis] - 1e-4f);
            EXPECT_LE(actual[axis], bounds.max[axis] + 1e-4f);
        }
    }
    for (const auto &matrix : reference) {
        NearMatrix(matrix, Matrix4::Identity());
    }
}

TEST(MeshSkinCook, NonuniformMirrorUsesLinearTangentsAndFlipsWinding)
{
    CookMeshSource      source;
    MeshSourcePrimitive primitive;
    primitive.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    primitive.normals.assign(3, Vector3{1, 1, 1} / std::sqrt(3.f));
    primitive.tangents.assign(3, Vector4{1 / std::sqrt(2.f), -1 / std::sqrt(2.f), 0, 1});
    primitive.indices = {0, 1, 2};
    Transform transform;
    transform.scale       = {-2, 3, 1};
    transform.translation = {4, 0, 0};
    primitive.transform   = transform.ToMatrix();
    source.primitives.push_back(primitive);
    MeshBuildConfig config;
    CookedMesh      mesh;
    ASSERT_TRUE(MeshAssembler({&source, &config, &mesh}).DoWork());
    const auto tangent = Read<Vector4>(mesh, VertexSemantic::TANGENT, 0);
    EXPECT_NEAR(tangent.x, -2 / std::sqrt(13.f), 1e-5f);
    EXPECT_NEAR(tangent.y, -3 / std::sqrt(13.f), 1e-5f);
    EXPECT_FLOAT_EQ(tangent.w, -1.f);
    const auto normal = Read<Vector3>(mesh, VertexSemantic::NORMAL, 0);
    EXPECT_NEAR(normal.x * tangent.x + normal.y * tangent.y + normal.z * tangent.z, 0, 1e-5f);
    const std::array<uint16_t, 3> expected{0, 2, 1};
    EXPECT_EQ(std::memcmp(mesh.indexData.data(), expected.data(), sizeof(expected)), 0);
    auto second          = primitive;
    second.materialIndex = 1;
    for (auto &position : second.positions) {
        position.x += 100;
    }
    source.primitives.push_back(second);
    ASSERT_TRUE(MeshAssembler({&source, &config, &mesh}).DoWork());
    ASSERT_TRUE(MeshOptimizer({&mesh}).DoWork());
    for (size_t sub = 0; sub < mesh.subMeshes.size(); ++sub) {
        for (size_t index = mesh.subMeshes[sub].indexOffset; index < mesh.subMeshes[sub].indexOffset + mesh.subMeshes[sub].indexCount; ++index) {
            uint16_t vertex = 0;
            std::memcpy(&vertex, mesh.indexData.data() + index * 2, 2);
            const auto point = Read<Vector3>(mesh, VertexSemantic::POSITION, vertex);
            EXPECT_EQ(point.x < -100, sub == 1);
        }
    }
}

TEST(MeshSkinCook, BuilderPublishesOneMeshTwoSkinsAndStableRigProducts)
{
    std::error_code error;
    const auto      root = std::filesystem::temp_directory_path(error) / ("sky-mesh-builder-" + Uuid::Create().ToString());
    ASSERT_FALSE(error);
    {
        CounterPtr<MeshProductFiles> fs    = new MeshProductFiles(FilePath(root));
        const auto                   bytes = Fixture();
        ASSERT_TRUE(fs->WriteFileExclusive("character.gltf", bytes));
        const std::string presets = R"({"defaultBundle":"mesh-skin-test","bundles":{"mesh-skin-test":{"optimize":true}}})";
        ASSERT_TRUE(fs->WriteFileExclusive("mesh_build_presets.json",
                                           std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(presets.data()), presets.size())));
        auto *database = AssetDataBase::Get();
        database->Reset();
        database->SetWorkSpaceFs(fs);
        const auto source = database->RegisterAsset("character.gltf", false);
        ASSERT_NE(source, nullptr);
        auto *manager = AssetManager::Get();
        manager->RegisterAssetHandler<Mesh>();
        manager->RegisterAssetHandler<Skin>();
        manager->RegisterAssetHandler<Skeleton>();
        manager->AddAssetProductBundle(new HashedAssetBundle(fs, "mesh-skin-test"));
        manager->SetSourceCatalog(database);
        AuroraMeshBuilder builder;
        builder.LoadConfig(fs);
        AssetBuildRequest request;
        request.assetInfo = source;
        request.file      = fs->OpenFile("character.gltf");
        request.target = request.bundle = "mesh-skin-test";
        AssetBuildResult result{};
        builder.Request(request, result);
        ASSERT_EQ(result.retCode, AssetBuildRetCode::SUCCESS) << result.error;
        std::map<std::string, Uuid> defaults;
        ASSERT_TRUE(database->GetGeneratedDefaults(source->uuid, defaults));
        const auto mesh = manager->LoadAsset<Mesh>(source->uuid);
        ASSERT_NE(mesh, nullptr);
        mesh->BlockUntilLoaded();
        ASSERT_TRUE(mesh->IsLoaded());
        std::vector<Uuid> products{source->uuid};
        database->ForEachGenerated([&](const GeneratedAssetRecord &record) {
            if (record.owner == source->uuid && record.active)
                products.push_back(record.uuid);
        });
        ASSERT_EQ(products.size(), 5U);
        AssetProductInspection inspection;
        ASSERT_TRUE(manager->InspectProduct(source->uuid, request.bundle, inspection));
        EXPECT_EQ(inspection.type, "AuroraMesh");
        EXPECT_TRUE(inspection.dependencies.empty());
        for (const auto &id : products) {
            EXPECT_TRUE(fs->FileExist(ProductFilePath(id)));
            const auto loaded = manager->LoadAsset(id);
            ASSERT_NE(loaded, nullptr);
            loaded->BlockUntilLoaded();
            EXPECT_TRUE(loaded->IsLoaded());
            EXPECT_TRUE(loaded->GetInstanceId());
        }
        builder.Request(request, result);
        ASSERT_EQ(result.retCode, AssetBuildRetCode::SUCCESS) << result.error;
        std::map<std::string, Uuid> recooked;
        ASSERT_TRUE(database->GetGeneratedDefaults(source->uuid, recooked));
        EXPECT_EQ(defaults, recooked);
        const auto second = manager->LoadAsset<Mesh>(source->uuid);
        ASSERT_NE(second, nullptr);
        second->BlockUntilLoaded();
        ASSERT_TRUE(second->IsLoaded());
        EXPECT_NE(second.get(), mesh.get());
        EXPECT_NE(second->GetInstanceId(), mesh->GetInstanceId());
        EXPECT_NE(second->GetRevision(), mesh->GetRevision());
        EXPECT_EQ(mesh->ReadBulk(mesh->Data().vertexOffset, mesh->Data().vertexSize),
                  second->ReadBulk(second->Data().vertexOffset, second->Data().vertexSize));
        const auto otherFs = fs->CreateSubSystem("other", true);
        manager->AddAssetProductBundle(new HashedAssetBundle(otherFs, "mesh-skin-second"));
        request.bundle = "mesh-skin-second";
        builder.Request(request, result);
        ASSERT_EQ(result.retCode, AssetBuildRetCode::SUCCESS) << result.error;
        AssetProductInspection otherProduct;
        ASSERT_TRUE(manager->InspectProduct(source->uuid, request.bundle, otherProduct));
        EXPECT_EQ(otherProduct.type, "AuroraMesh");
        for (const auto &id : products)
            EXPECT_TRUE(otherFs->FileExist(ProductFilePath(id)));
        std::map<std::string, Uuid> otherDefaults;
        ASSERT_TRUE(database->GetGeneratedDefaults(source->uuid, otherDefaults));
        EXPECT_EQ(otherDefaults, defaults);
        const auto beforeFailure = manager->LoadAsset<Mesh>(source->uuid);
        ASSERT_NE(beforeFailure, nullptr);
        beforeFailure->BlockUntilLoaded();
        ASSERT_TRUE(beforeFailure->IsLoaded());
        request.bundle = "mesh-skin-test";
        fs->fail       = true;
        builder.Request(request, result);
        EXPECT_EQ(result.retCode, AssetBuildRetCode::FAILED);
        EXPECT_EQ(manager->LoadAsset<Mesh>(source->uuid).get(), beforeFailure.get());
        manager->SetSourceCatalog(nullptr);
        database->Reset();
        AssetExecutor::Get()->WaitForAll();
    }
    std::filesystem::remove_all(root, error);
    EXPECT_FALSE(error) << error.message();
}

TEST(MeshSkinCook, StandaloneSkeletonRecookNeverMutatesLiveSnapshot)
{
    std::error_code error;
    const auto      root = std::filesystem::temp_directory_path(error) / ("sky-rig-builder-" + Uuid::Create().ToString());
    ASSERT_FALSE(error);
    {
        CounterPtr<MeshProductFiles> fs      = new MeshProductFiles(FilePath(root));
        auto                        *manager = AssetManager::Get();
        manager->AddAssetProductBundle(new HashedAssetBundle(fs, "rig-snapshot-test"));
        AssetBuildRequest request;
        request.assetInfo       = new AssetSourceInfo;
        request.assetInfo->uuid = Uuid::Create();
        request.assetInfo->path = "test.skeleton";
        request.assetInfo->ext  = ".skeleton";
        request.bundle = request.target = "rig-snapshot-test";
        const auto write                = [&](int translation, int parent) {
            const auto json = "{\"bones\":[{\"name\":\"root\",\"parent\":" + std::to_string(parent) + ",\"translation\":[" +
                              std::to_string(translation) + ",0,0]}]}";
            request.file      = fs->CreateOrOpenFile("test.skeleton");
            const auto output = request.file->WriteAsArchive();
            EXPECT_TRUE(output->SaveRaw(json.data(), json.size()));
        };
        AuroraSkeletonBuilder builder;
        AssetBuildResult      result{};
        write(1, -1);
        builder.Request(request, result);
        ASSERT_EQ(result.retCode, AssetBuildRetCode::SUCCESS) << result.error;
        const auto first = manager->LoadAsset<Skeleton>(request.assetInfo->uuid);
        ASSERT_NE(first, nullptr);
        first->BlockUntilLoaded();
        ASSERT_TRUE(first->IsLoaded());
        const auto firstRig = FetchOrCreateSkeletonByAsset(first);
        write(2, -1);
        builder.Request(request, result);
        ASSERT_EQ(result.retCode, AssetBuildRetCode::SUCCESS) << result.error;
        const auto second = manager->LoadAsset<Skeleton>(request.assetInfo->uuid);
        ASSERT_NE(second, nullptr);
        second->BlockUntilLoaded();
        ASSERT_TRUE(second->IsLoaded());
        EXPECT_NE(first.get(), second.get());
        EXPECT_FLOAT_EQ(firstRig->GetRefPos()->transforms[0].translation.x, 1.f);
        EXPECT_FLOAT_EQ(FetchOrCreateSkeletonByAsset(second)->GetRefPos()->transforms[0].translation.x, 2.f);
        write(3, 0);
        builder.Request(request, result);
        EXPECT_EQ(result.retCode, AssetBuildRetCode::FAILED);
        EXPECT_EQ(manager->LoadAsset<Skeleton>(request.assetInfo->uuid).get(), second.get());
        fs->fail = true;
        write(4, -1);
        builder.Request(request, result);
        EXPECT_EQ(result.retCode, AssetBuildRetCode::FAILED);
        EXPECT_EQ(manager->LoadAsset<Skeleton>(request.assetInfo->uuid).get(), second.get());
        AssetExecutor::Get()->WaitForAll();
    }
    std::filesystem::remove_all(root, error);
    EXPECT_FALSE(error) << error.message();
}

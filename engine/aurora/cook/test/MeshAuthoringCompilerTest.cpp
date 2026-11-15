#include <aurora/adaptor/assets/MeshAsset.h>
#include <aurora/adaptor/assets/SkeletonAsset.h>
#include <aurora/adaptor/assets/SkinAsset.h>
#include <aurora/cook/mesh/AuroraMeshBuilder.h>
#include <aurora/cook/mesh/MeshAuthoringCompiler.h>
#include <aurora/cook/mesh/MeshBuildConfig.h>
#include <framework/asset/AssetManager.h>
#include <gtest/gtest.h>
#include <physics/softbody/IMeshAuthoringCompiler.h>

#include <filesystem>

using namespace sky;
using namespace sky::phy;
using namespace sky::aurora;

namespace {
    AuthoringMeshSource Triangle()
    {
        AuthoringMeshSource source;
        source.key           = "body";
        source.materialNames = {"body"};
        AuthoringMeshPrimitive primitive;
        primitive.key       = "body/primitive/0";
        primitive.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
        primitive.normals   = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
        primitive.indices   = {0, 1, 2};
        source.primitives.push_back(std::move(primitive));
        return source;
    }
    AuthoringMeshBinding Binding()
    {
        AuthoringMeshBinding binding;
        binding.materialSlots                  = {Uuid::Create()};
        binding.layoutSignature                = 1234;
        binding.bones                          = {{"root", UINT16_MAX, {}}, {"control", 0, {}}};
        binding.bones[1].reference.translation = {0, 2, 0};
        AuthoringPrimitiveBinding primitive;
        primitive.key = "body/primitive/0";
        primitive.vertices.resize(3);
        for (auto &vertex : primitive.vertices) {
            vertex.bones   = {1, 0, 0, 0};
            vertex.weights = {1, 0, 0, 0};
        }
        binding.primitives.push_back(std::move(primitive));
        return binding;
    }
    AuthoringProductIds Ids()
    {
        return {Uuid::Create(), Uuid::Create(), Uuid::Create(), Uuid::Create()};
    }
} // namespace

TEST(MeshAuthoringCompilerTest, ProductsPreserveBindReferenceAndVertexMapping)
{
    auto *manager = AssetManager::Get();
    manager->RegisterAssetHandler<Mesh>();
    manager->RegisterAssetHandler<Skin>();
    manager->RegisterAssetHandler<Skeleton>();
    const auto            compiler = cook::CreateMeshAuthoringCompiler();
    const auto            ids      = Ids();
    AuthoringMeshProducts out;
    const auto            status = compiler->Compile(Triangle(), Binding(), ids, out);
    ASSERT_TRUE(status) << status.message;
    ASSERT_EQ(out.products.size(), 3u);
    EXPECT_EQ(out.products[0]->GetUuid(), ids.mesh);
    EXPECT_EQ(out.products[1]->GetUuid(), ids.skin);
    EXPECT_EQ(out.products[2]->GetUuid(), ids.skeleton);
    EXPECT_EQ(out.vertexSources, (std::vector<std::array<uint32_t, 2>>{{0, 0}, {0, 1}, {0, 2}}));
    ASSERT_EQ(out.products[1]->GetType(), Name("AuroraSkin"));
    const auto skin = std::static_pointer_cast<Asset<Skin>>(out.products[1]);
    ASSERT_TRUE(Skin::Validate(skin->Data().binding));
    ASSERT_EQ(skin->Data().binding.binds.size(), 2u);
    const auto product  = Binding().bones[1].reference.ToMatrix() * skin->Data().binding.binds[1].inverseBind;
    const auto identity = Matrix4::Identity();
    for (size_t i = 0; i < 16; ++i) {
        EXPECT_NEAR(product.v[i], identity.v[i], 1e-6f);
    }
    EXPECT_EQ(skin->Data().sourceOwner, ids.sourceOwner);
    auto zeroSlots = Binding();
    for (auto &vertex : zeroSlots.primitives[0].vertices) {
        vertex.bones = {1, 1, 1, 1};
    }
    AuthoringMeshProducts canonical;
    EXPECT_TRUE(compiler->Compile(Triangle(), zeroSlots, ids, canonical));
}

TEST(MeshAuthoringCompilerTest, InvalidWeightsOrIdentityLeaveOutputIntact)
{
    auto *manager = AssetManager::Get();
    manager->RegisterAssetHandler<Mesh>();
    manager->RegisterAssetHandler<Skin>();
    manager->RegisterAssetHandler<Skeleton>();
    const auto            compiler = cook::CreateMeshAuthoringCompiler();
    const auto            ids      = Ids();
    AuthoringMeshProducts out;
    ASSERT_TRUE(compiler->Compile(Triangle(), Binding(), ids, out));
    const auto previous                    = out.products;
    auto       bad                         = Binding();
    bad.primitives[0].vertices[0].bones[0] = 256;
    EXPECT_FALSE(compiler->Compile(Triangle(), bad, ids, out));
    EXPECT_EQ(out.products, previous);
    bad                                      = Binding();
    bad.primitives[0].vertices[0].weights[0] = 0.7f;
    EXPECT_FALSE(compiler->Compile(Triangle(), bad, ids, out));
    EXPECT_EQ(out.products, previous);
    bad                   = Binding();
    bad.primitives[0].key = "wrong";
    EXPECT_FALSE(compiler->Compile(Triangle(), bad, ids, out));
    EXPECT_EQ(out.products, previous);
    bad                 = Binding();
    bad.bones[1].parent = 1;
    EXPECT_FALSE(compiler->Compile(Triangle(), bad, ids, out));
    EXPECT_EQ(out.products, previous);
}

TEST(MeshAuthoringCompilerTest, MaterialSlotsPreservePartAssignmentsAndProductDependencies)
{
    auto *manager = AssetManager::Get();
    manager->RegisterAssetHandler<Mesh>();
    manager->RegisterAssetHandler<Skin>();
    manager->RegisterAssetHandler<Skeleton>();
    const auto compiler  = cook::CreateMeshAuthoringCompiler();
    auto       source    = Triangle();
    auto       binding   = Binding();
    source.materialNames = {"body", "eye", "pupil"};
    const std::array<uint32_t, 5> slots{0, 1, 1, 2, 2};
    source.primitives.resize(slots.size(), source.primitives.front());
    binding.primitives.resize(slots.size(), binding.primitives.front());
    for (size_t i = 0; i < slots.size(); ++i) {
        source.primitives[i].key = binding.primitives[i].key = "body/primitive/" + std::to_string(i);
        source.primitives[i].materialIndex                   = slots[i];
    }
    binding.materialSlots     = {Uuid::Create(), Uuid::Create(), Uuid::Create()};
    const auto            ids = Ids();
    AuthoringMeshProducts out;
    ASSERT_TRUE(compiler->Compile(source, binding, ids, out));
    const auto mesh = std::static_pointer_cast<Asset<Mesh>>(out.products.front());
    EXPECT_EQ(mesh->Data().materials, binding.materialSlots);
    EXPECT_EQ(out.effectiveDependencies, binding.materialSlots);
    ASSERT_EQ(mesh->Data().subMeshes.size(), slots.size());
    for (size_t i = 0; i < slots.size(); ++i)
        EXPECT_EQ(mesh->Data().subMeshes[i].materialIndex, slots[i]);

    std::error_code error;
    const auto      temporary = std::filesystem::temp_directory_path(error);
    ASSERT_FALSE(error);
    const auto directory = temporary / ("sky-authoring-materials-" + Uuid::Create().ToString());
    struct Cleanup {
        std::filesystem::path directory;
        std::filesystem::path parent;
        ~Cleanup()
        {
            std::error_code boundary;
            if (!std::filesystem::equivalent(directory.parent_path(), parent, boundary) || boundary) {
                ADD_FAILURE() << "Temporary product directory escaped its owner";
                return;
            }
            std::error_code removal;
            std::filesystem::remove_all(directory, removal);
            EXPECT_FALSE(removal);
        }
    } cleanup{directory, temporary};
    AssetManager storage;
    storage.RegisterAssetHandler<Mesh>();
    const NativeFileSystemPtr fs = new NativeFileSystem(FilePath(directory));
    storage.AddAssetProductBundle(new HashedAssetBundle(fs, "materials"));
    ASSERT_TRUE(storage.SaveAsset(mesh, "materials"));
    AssetProductInspection inspection;
    ASSERT_TRUE(storage.InspectProduct(ids.mesh, "materials", inspection));
    EXPECT_EQ(inspection.dependencies, binding.materialSlots);

    binding.materialSlots[2] = binding.materialSlots[0];
    AuthoringMeshProducts duplicate;
    ASSERT_TRUE(compiler->Compile(source, binding, Ids(), duplicate));
    EXPECT_EQ(duplicate.effectiveDependencies, (std::vector<Uuid>{binding.materialSlots[0], binding.materialSlots[1]}));
    ASSERT_TRUE(storage.SaveAsset(duplicate.products.front(), "materials"));
    ASSERT_TRUE(storage.InspectProduct(duplicate.products.front()->GetUuid(), "materials", inspection));
    EXPECT_EQ(inspection.dependencies, (std::vector<Uuid>{binding.materialSlots[0], binding.materialSlots[1]}));
}

TEST(MeshAuthoringCompilerTest, CompilationRequiresExplicitMaterialsAndReportsEffectiveDependencies)
{
    auto *manager = AssetManager::Get();
    manager->RegisterAssetHandler<Mesh>();
    manager->RegisterAssetHandler<Skin>();
    manager->RegisterAssetHandler<Skeleton>();
    const auto            compiler = cook::CreateMeshAuthoringCompiler();
    const auto            ids      = Ids();
    auto                  source   = Triangle();
    auto                  binding  = Binding();
    AuthoringMeshProducts out;
    binding.materialSlots.clear();
    EXPECT_FALSE(compiler->Compile(source, binding, ids, out));
    EXPECT_TRUE(out.products.empty());
    const auto fallback = Uuid::Create();
    binding.materialSlots.assign(8, fallback);
    source.primitives.front().materialIndex = 7;
    ASSERT_TRUE(compiler->Compile(source, binding, ids, out));
    EXPECT_EQ(std::static_pointer_cast<Asset<Mesh>>(out.products.front())->Data().materials, std::vector<Uuid>(8, fallback));
    EXPECT_EQ(out.effectiveDependencies, std::vector<Uuid>{fallback});
    const auto previous        = out.products;
    const auto previousSources = out.vertexSources;
    const auto previousLayout  = out.skeletonLayout;
    binding.materialSlots      = {Uuid::Create()};
    EXPECT_FALSE(compiler->Compile(source, binding, ids, out));
    source.primitives.front().materialIndex = 0;
    for (const auto &invalid : std::array<Uuid, 5>{Uuid{}, ids.sourceOwner, ids.mesh, ids.skin, ids.skeleton}) {
        binding.materialSlots.front() = invalid;
        EXPECT_FALSE(compiler->Compile(source, binding, ids, out));
        EXPECT_EQ(out.products, previous);
        EXPECT_EQ(out.vertexSources, previousSources);
        EXPECT_EQ(out.skeletonLayout, previousLayout);
    }
}

TEST(MeshAuthoringCompilerTest, RegistryRejectsDuplicateAndForeignUnregistration)
{
    auto      *registry = MeshAuthoringCompilerRegistry::Get();
    const auto first    = cook::CreateMeshAuthoringCompiler();
    const auto second   = cook::CreateMeshAuthoringCompiler();
    EXPECT_FALSE(registry->Register({}));
    EXPECT_TRUE(registry->Unregister({}));
    ASSERT_TRUE(registry->Register(first));
    EXPECT_FALSE(registry->Register(first));
    EXPECT_FALSE(registry->Register(second));
    EXPECT_FALSE(registry->Unregister(second));
    const std::shared_ptr<IMeshAuthoringCompiler> foreignOwner(first.get(), [](IMeshAuthoringCompiler *) {});
    EXPECT_FALSE(registry->Unregister(foreignOwner));
    EXPECT_FALSE(registry->Unregister(std::shared_ptr<IMeshAuthoringCompiler>(first, nullptr)));
    EXPECT_EQ(registry->Acquire(), first);
    EXPECT_TRUE(registry->Unregister(first));
    EXPECT_FALSE(registry->Acquire());
    EXPECT_TRUE(registry->Register(second));
    EXPECT_TRUE(registry->Unregister(second));
}

TEST(MeshAuthoringCompilerTest, AcquiredCompilerSurvivesUnregistrationAndReplacement)
{
    auto                                       *registry = MeshAuthoringCompilerRegistry::Get();
    auto                                        first    = cook::CreateMeshAuthoringCompiler();
    const std::weak_ptr<IMeshAuthoringCompiler> weak     = first;
    ASSERT_TRUE(registry->Register(first));
    auto acquired = registry->Acquire();
    ASSERT_EQ(acquired, first);
    ASSERT_TRUE(registry->Unregister(first));
    first.reset();
    EXPECT_FALSE(weak.expired());
    const auto replacement = cook::CreateMeshAuthoringCompiler();
    EXPECT_TRUE(registry->Register(replacement));
    EXPECT_EQ(registry->Acquire(), replacement);
    EXPECT_FALSE(registry->Unregister(acquired));
    EXPECT_EQ(registry->Acquire(), replacement);
    acquired.reset();
    EXPECT_TRUE(weak.expired());
    EXPECT_TRUE(registry->Unregister(replacement));
}

TEST(MeshAuthoringCompilerTest, ExplicitMaterialPolicyPreparesSlotsBeforeCompilation)
{
    const auto fallback                        = Uuid::Create();
    const auto presets                         = std::make_shared<cook::MeshBuildPresets>();
    presets->defaultBundle                     = "common";
    presets->bundles["common"].defaultMaterial = fallback;
    const auto        compiler                 = cook::CreateMeshAuthoringCompiler(presets);
    std::vector<Uuid> slots;
    ASSERT_TRUE(compiler->ResolveMaterials("common", 3, {}, slots));
    EXPECT_EQ(slots, std::vector<Uuid>(3, fallback));
    const auto              explicitMaterial = Uuid::Create();
    const std::vector<Uuid> explicitSlots(3, explicitMaterial);
    ASSERT_TRUE(compiler->ResolveMaterials("common", 3, explicitSlots, slots));
    EXPECT_EQ(slots, explicitSlots);
    const auto previous = slots;
    EXPECT_FALSE(compiler->ResolveMaterials("common", 8193, {}, slots));
    EXPECT_EQ(slots, previous);
    presets->bundles.clear();
    EXPECT_FALSE(compiler->ResolveMaterials("common", 3, {}, slots));
    EXPECT_EQ(slots, previous);
    EXPECT_TRUE(compiler->ResolveMaterials("common", 3, explicitSlots, slots));
}

TEST(MeshAuthoringCompilerTest, SharedPolicyRejectsMalformedConfigAndMissingFileRestoresDefaults)
{
    std::error_code error;
    const auto      temporary = std::filesystem::temp_directory_path(error);
    ASSERT_FALSE(error);
    const auto root = temporary / ("sky-mesh-policy-" + Uuid::Create().ToString());
    struct Cleanup {
        std::filesystem::path root;
        ~Cleanup()
        {
            std::error_code error;
            std::filesystem::remove_all(root, error);
            EXPECT_FALSE(error);
        }
    } cleanup{root};
    const FileSystemPtr files   = new NativeFileSystem(FilePath(root));
    const auto          presets = std::make_shared<cook::MeshBuildPresets>();
    AuroraMeshBuilder   builder(presets);
    const auto          compiler = cook::CreateMeshAuthoringCompiler(presets);
    ASSERT_TRUE(files->WriteFileExclusive("mesh_build_presets.json", std::vector<uint8_t>{'{'}));
    builder.LoadConfig(files);
    ASSERT_FALSE(presets->loadError.empty());
    const std::vector<Uuid> supplied{Uuid::Create()};
    std::vector<Uuid>       out      = supplied;
    const auto              rejected = compiler->ResolveMaterials("common", 1, supplied, out);
    EXPECT_FALSE(rejected);
    EXPECT_EQ(rejected.message, presets->loadError);
    EXPECT_EQ(out, supplied);
    EXPECT_FALSE(compiler->ResolveMaterials("common", 1, {}, out));
    AssetBuildRequest request;
    request.assetInfo = new AssetSourceInfo;
    request.file      = files->OpenFile("mesh_build_presets.json");
    AssetBuildResult result;
    builder.Request(request, result);
    EXPECT_EQ(result.retCode, AssetBuildRetCode::FAILED);
    EXPECT_EQ(result.error, presets->loadError);
    ASSERT_TRUE(files->RemoveFile("mesh_build_presets.json"));
    builder.LoadConfig(files);
    EXPECT_TRUE(presets->loadError.empty());
    EXPECT_EQ(presets->defaultBundle, "common");
    ASSERT_EQ(presets->bundles.size(), 1u);
    EXPECT_FALSE(presets->bundles.at("common").defaultMaterial);
    EXPECT_TRUE(compiler->ResolveMaterials("common", 1, supplied, out));
}

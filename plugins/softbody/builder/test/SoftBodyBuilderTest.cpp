#include <softbody/builder/SoftBodyBuilder.h>

#include <aurora/adaptor/assets/MaterialAsset.h>
#include <aurora/adaptor/assets/MeshAsset.h>
#include <aurora/adaptor/assets/SceneAssets.h>
#include <aurora/adaptor/assets/SkeletonAsset.h>
#include <aurora/adaptor/assets/SkinAsset.h>
#include <aurora/cook/mesh/MeshAuthoringCompiler.h>
#include <aurora/cook/mesh/MeshBuildConfig.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetProduct.h>
#include <gtest/gtest.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <softbody/assets/SoftBodyAsset.h>
#include <softbody/assets/SoftBodyRecipe.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>

using namespace sky;
using namespace sky::softbody;

namespace {
    class Compiler final : public phy::IMeshAuthoringCompiler {
    public:
        Compiler()
        {
            const auto presets         = std::make_shared<aurora::cook::MeshBuildPresets>();
            presets->defaultBundle     = "common";
            presets->bundles["common"] = {};
            delegate                   = aurora::cook::CreateMeshAuthoringCompiler(presets);
        }
        bool                                         fail = false;
        std::shared_ptr<phy::IMeshAuthoringCompiler> delegate;
        phy::PhysicsStatus
        ResolveMaterials(const std::string &target, uint32_t count, std::span<const Uuid> supplied, std::vector<Uuid> &out) const override
        {
            return delegate->ResolveMaterials(target, count, supplied, out);
        }
        phy::PhysicsStatus
        Load(std::span<const uint8_t> bytes, const std::string &extension, std::vector<phy::AuthoringMeshSource> &out) const override
        {
            return delegate->Load(bytes, extension, out);
        }
        phy::PhysicsStatus Compile(const phy::AuthoringMeshSource  &source,
                                   const phy::AuthoringMeshBinding &binding,
                                   const phy::AuthoringProductIds  &ids,
                                   phy::AuthoringMeshProducts      &out) const override
        {
            if (fail) {
                return {phy::PhysicsError::InvalidState, "injected compiler failure"};
            }
            return delegate->Compile(source, binding, ids, out);
        }
    };

    void CookRoundTrip(const std::vector<uint8_t> &bytes, const std::string &extension, bool seal, bool supplied = false)
    {
        std::error_code error;
        const auto      root = std::filesystem::temp_directory_path(error) / ("sky-softbody-cook-" + Uuid::Create().ToString());
        ASSERT_FALSE(error);
        {
            NativeFileSystemPtr sources  = new NativeFileSystem(FilePath(root / "assets"));
            NativeFileSystemPtr products = new NativeFileSystem(FilePath(root / "products"));
            auto               *database = AssetDataBase::Get();
            auto               *manager  = AssetManager::Get();
            database->Reset();
            database->SetWorkSpaceFs(sources);
            const NativeFileSystemPtr engine = new NativeFileSystem(FilePath(root / "engine"));
            database->SetEngineFs(engine);
            ASSERT_TRUE(engine->CreateSubSystem("assets", true)->WriteFileExclusive(FilePath(aurora::scene_assets::UNLIT), std::vector<uint8_t>{1}));
            const auto fallback = database->RegisterAsset("engine", FilePath(aurora::scene_assets::UNLIT), false);
            ASSERT_TRUE(fallback);
            manager->SetSourceCatalog(database);
            const std::string bundle = "softbody-" + Uuid::Create().ToString();
            manager->AddAssetProductBundle(new HashedAssetBundle(products, bundle));
            const auto material = manager->CreateBuildAsset(fallback->uuid, Name("AuroraMaterial"));
            ASSERT_TRUE(material);
            ASSERT_TRUE(manager->SaveAsset(material, bundle));
            const auto compiler = std::make_shared<Compiler>();
            ASSERT_TRUE(phy::MeshAuthoringCompilerRegistry::Get()->Register(compiler));
            struct Cleanup {
                std::shared_ptr<Compiler> compiler;
                ~Cleanup()
                {
                    phy::MeshAuthoringCompilerRegistry::Get()->Unregister(compiler);
                    AssetExecutor::Get()->WaitForAll();
                    AssetManager::Get()->SetSourceCatalog(nullptr);
                    AssetDataBase::Get()->Reset();
                    AssetDataBase::Get()->SetEngineFs({});
                    AssetDataBase::Get()->SetWorkSpaceFs({});
                }
            } cleanup{compiler};
            const auto sourceName = "animal" + extension;
            ASSERT_TRUE(sources->WriteFileExclusive(sourceName, bytes));
            const auto original = database->RegisterAsset(sourceName, false);
            ASSERT_NE(original, nullptr);
            std::vector<phy::AuthoringMeshSource> meshes;
            ASSERT_TRUE(compiler->Load(bytes, extension, meshes));
            ASSERT_EQ(meshes.size(), 1u);
            const auto &mesh = meshes.front();
            const auto  body = std::max_element(mesh.primitives.begin(), mesh.primitives.end(),
                                                [](const auto &a, const auto &b) { return a.indices.size() < b.indices.size(); });
            ASSERT_NE(body, mesh.primitives.end());
            SoftBodyRecipe recipe;
            recipe.source                               = original->uuid;
            recipe.sourceKey                            = mesh.key;
            recipe.settings.bodyPrimitive               = body->key;
            recipe.settings.generator.useExplicitCenter = true;
            recipe.settings.generator.cageCenter        = seal ? Vector3{-.10f, .12f, 0.f} : VEC3_ZERO;
            if (seal) {
                recipe.controls = {{"root-anchor", {0.f, .20f, 0.f}, phy::SoftBodyAttachmentMode::Dynamic},
                                   {"head-anchor", {.25f, .28f, 0.f}, phy::SoftBodyAttachmentMode::Dynamic}};
            } else {
                recipe.settings.generator.maxSurfaceDistance = .15f;
                recipe.settings.generator.maxVolumeExcess    = 4.f;
                recipe.controls                              = {{"head-anchor", {0.f, .05f, 0.f}, phy::SoftBodyAttachmentMode::RootKinematic}};
            }
            for (const auto &primitive : mesh.primitives) {
                if (primitive.key != body->key) {
                    recipe.settings.attachments.push_back({primitive.key, "head-anchor"});
                }
            }
            if (supplied) {
                recipe.settings.generator.mode                 = RigGenerationMode::SuppliedCenterFan;
                recipe.settings.generator.centerIndex          = 6;
                recipe.settings.generator.bindingPositionScale = .7f;
                recipe.settings.generator.maxControlPoints     = 7;
                recipe.settings.normalPolicy                   = SoftBodyNormalPolicy::StandardSkinning;
                recipe.controls                      = {{"px", {.3f, 0, 0}}, {"nx", {-.3f, 0, 0}}, {"py", {0, .3f, 0}},  {"ny", {0, -.3f, 0}},
                                                        {"pz", {0, 0, .3f}}, {"nz", {0, 0, -.3f}}, {"center", {0, 0, 0}}};
                recipe.settings.generator.tetrahedra = {{6, 0, 2, 4}, {6, 2, 1, 4}, {6, 1, 3, 4}, {6, 3, 0, 4},
                                                        {6, 2, 0, 5}, {6, 1, 2, 5}, {6, 3, 1, 5}, {6, 0, 3, 5}};
            }
            const auto file = sources->CreateOrOpenFile("animal.softbodyrig");
            ASSERT_NE(file, nullptr);
            const auto writeRecipe = [&] {
                std::string text;
                EXPECT_TRUE(WriteSoftBodyRecipe(recipe, text));
                const auto archive = file->WriteAsArchive();
                EXPECT_TRUE(archive && archive->SaveRaw(text.data(), text.size()));
                if (archive)
                    archive->Flush();
            };
            writeRecipe();
            const auto owner = database->RegisterAsset("animal.softbodyrig", false);
            ASSERT_NE(owner, nullptr);
            AssetBuildRequest request;
            request.assetInfo = owner;
            request.file      = file;
            request.bundle = request.target = bundle;
            SoftBodyBuilder  builder;
            AssetBuildResult result;
            builder.Request(request, result);
            ASSERT_EQ(result.retCode, AssetBuildRetCode::SUCCESS) << result.error;
            const auto rig = manager->LoadAsset<SoftBodyAsset>(owner->uuid);
            ASSERT_NE(rig, nullptr);
            rig->BlockUntilLoaded();
            ASSERT_TRUE(rig->IsLoaded());
            ASSERT_TRUE(ValidateSoftBodyAsset(rig->Data()));
            const auto &data = rig->Data();
            EXPECT_LE(data.body.mesh.vertices.size(), 30u);
            EXPECT_EQ(data.normalPolicy, supplied ? SoftBodyNormalPolicy::StandardSkinning : SoftBodyNormalPolicy::TetGradient);
            if (supplied) {
                EXPECT_EQ(data.body.mesh.vertices.size(), 7u);
                EXPECT_EQ(data.body.mesh.tetrahedra.size(), 8u);
                EXPECT_EQ(data.controlIds, (std::vector<std::string>{"px", "nx", "py", "ny", "pz", "nz", "center"}));
            }
            const auto skin     = manager->LoadAsset<aurora::Skin>(data.skin);
            const auto skeleton = manager->LoadAsset<Skeleton>(data.skeleton);
            const auto rendered = manager->LoadAsset<aurora::Mesh>(data.mesh);
            ASSERT_NE(skin, nullptr);
            ASSERT_NE(skeleton, nullptr);
            ASSERT_NE(rendered, nullptr);
            skin->BlockUntilLoaded();
            skeleton->BlockUntilLoaded();
            rendered->BlockUntilLoaded();
            ASSERT_TRUE(skin->IsLoaded());
            ASSERT_TRUE(skeleton->IsLoaded());
            ASSERT_TRUE(rendered->IsLoaded());
            EXPECT_EQ(owner->dependencies, (std::vector<Uuid>{original->uuid, fallback->uuid}));
            EXPECT_FALSE(rendered->Data().materials.empty());
            for (const auto &id : rendered->Data().materials)
                EXPECT_EQ(id, fallback->uuid);
            AssetProductInspection meshProduct;
            ASSERT_TRUE(manager->InspectProduct(data.mesh, bundle, meshProduct));
            EXPECT_EQ(meshProduct.dependencies, std::vector<Uuid>{fallback->uuid});
            EXPECT_EQ(skeleton->Data().boneData.size(), data.body.mesh.vertices.size() + 1u);
            EXPECT_EQ(skin->Data().sourceOwner, owner->uuid);
            EXPECT_EQ(skin->Data().binding.targetLayoutSignature, data.skeletonLayout);
            std::map<std::string, Uuid> firstIds;
            ASSERT_TRUE(database->GetGeneratedDefaults(owner->uuid, firstIds));
            EXPECT_EQ(firstIds.at("mesh"), data.mesh);
            EXPECT_EQ(firstIds.at("softbody"), owner->uuid);
            std::vector<uint8_t> firstProduct;
            ASSERT_TRUE(products->OpenFile(ProductFilePath(owner->uuid))->ReadBin(firstProduct));
            compiler->fail = true;
            builder.Request(request, result);
            EXPECT_EQ(result.retCode, AssetBuildRetCode::FAILED);
            EXPECT_EQ(result.error, "injected compiler failure");
            std::vector<uint8_t> failedProduct;
            ASSERT_TRUE(products->OpenFile(ProductFilePath(owner->uuid))->ReadBin(failedProduct));
            EXPECT_EQ(failedProduct, firstProduct);
            EXPECT_EQ(manager->LoadAsset<SoftBodyAsset>(owner->uuid), rig);
            compiler->fail                         = false;
            recipe.settings.material.linearDamping = .2f;
            writeRecipe();
            builder.Request(request, result);
            ASSERT_EQ(result.retCode, AssetBuildRetCode::SUCCESS) << result.error;
            std::map<std::string, Uuid> againIds;
            ASSERT_TRUE(database->GetGeneratedDefaults(owner->uuid, againIds));
            EXPECT_EQ(againIds, firstIds);
            const auto again = manager->LoadAsset<SoftBodyAsset>(owner->uuid);
            ASSERT_NE(again, nullptr);
            again->BlockUntilLoaded();
            ASSERT_TRUE(again->IsLoaded());
            EXPECT_NE(again, rig);
            EXPECT_FLOAT_EQ(again->Data().body.linearDamping, .2f);
            EXPECT_FLOAT_EQ(rig->Data().body.linearDamping, .1f);
        }
        std::filesystem::remove_all(root, error);
        EXPECT_FALSE(error) << error.message();
    }
} // namespace

TEST(SoftBodyBuilder, ProductsReopenStableIdsAndFailedCompilePreservesPublishedSnapshot)
{
    const std::string obj = "o body\nv 0.2 0 0\nv -0.2 0 0\nv 0 0.2 0\nv 0 -0.2 0\nv 0 0 0.2\nv 0 0 -0.2\n"
                            "f 1 3 5\nf 3 2 5\nf 2 4 5\nf 4 1 5\nf 3 1 6\nf 2 3 6\nf 4 2 6\nf 1 4 6\n";
    CookRoundTrip({obj.begin(), obj.end()}, ".obj", false);
}

TEST(SoftBodyBuilder, OriginalSealArtifactUsesProductionImportGenerateCookAndReopen)
{
    const char *path = std::getenv("SKY_SOFTBODY_SEAL_GLB");
    if (!path)
        GTEST_SKIP() << "Optional original seal artifact";
    std::ifstream input(path, std::ios::binary);
    ASSERT_TRUE(input.good());
    CookRoundTrip({std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()}, ".glb", true);
}

TEST(SoftBodyBuilder, SuppliedCenterFanCooksOrderedControlsAndStandardSkinningPolicy)
{
    const std::string obj = "o body\nv 0.2 0 0\nv -0.2 0 0\nv 0 0.2 0\nv 0 -0.2 0\nv 0 0 0.2\nv 0 0 -0.2\n"
                            "f 1 3 5\nf 3 2 5\nf 2 4 5\nf 4 1 5\nf 3 1 6\nf 2 3 6\nf 4 2 6\nf 1 4 6\n";
    CookRoundTrip({obj.begin(), obj.end()}, ".obj", false, true);
}

TEST(SoftBodyBuilder, OriginalSuppliedArtifactUsesProductionImportBindingAndCompiler)
{
    const char *glbPath    = std::getenv("SKY_SOFTBODY_SUPPLIED_GLB");
    const char *recipePath = std::getenv("SKY_SOFTBODY_SUPPLIED_RECIPE");
    if (!glbPath || !recipePath)
        GTEST_SKIP() << "Optional external supplied cage fixture";
    const auto readText = [](const char *path) {
        std::ifstream stream(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    };
    SoftBodyRecipe recipe;
    const auto     parsed = ParseSoftBodyRecipe(readText(recipePath), recipe);
    ASSERT_TRUE(parsed) << parsed.message;
    ASSERT_EQ(recipe.settings.generator.mode, RigGenerationMode::SuppliedCenterFan);
    ASSERT_EQ(recipe.settings.normalPolicy, SoftBodyNormalPolicy::StandardSkinning);
    const auto                            compiler = aurora::cook::CreateMeshAuthoringCompiler();
    const auto                            bytes    = readText(glbPath);
    std::vector<phy::AuthoringMeshSource> sources;
    ASSERT_TRUE(compiler->Load({reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size()}, ".glb", sources));
    const auto source = std::find_if(sources.begin(), sources.end(), [&](const auto &s) { return s.key == recipe.sourceKey; });
    ASSERT_NE(source, sources.end());
    RigGenerationInput  input{*source, recipe.settings.bodyPrimitive, recipe.controls, recipe.settings.attachments, recipe.settings.generator};
    RigGenerationOutput rig;
    const auto          generated = GenerateRig(input, rig);
    ASSERT_TRUE(generated) << generated.message;
    EXPECT_EQ(rig.mesh.vertices.size(), 15u);
    EXPECT_EQ(rig.binding.bones.size(), 16u);
    EXPECT_EQ(rig.mesh.tetrahedra.size(), 24u);
    EXPECT_EQ(rig.mesh.edges.size(), 50u);
    EXPECT_TRUE(rig.diagnostics.coverageInBindingSpace);
    for (size_t i = 0; i < recipe.controls.size(); ++i) {
        EXPECT_EQ(rig.mesh.vertices[i].position, recipe.controls[i].position);
        EXPECT_EQ(rig.controlIds[i], recipe.controls[i].id);
    }
    rig.binding.materialSlots = recipe.settings.materialSlots;
    phy::AuthoringMeshProducts compiled;
    const auto cooked = compiler->Compile(*source, rig.binding, {Uuid::Create(), Uuid::Create(), Uuid::Create(), Uuid::Create()}, compiled);
    ASSERT_TRUE(cooked) << cooked.message;
    ASSERT_EQ(compiled.products.size(), 3u);
    std::cout << " supplied controls=" << rig.mesh.vertices.size() << " tets=" << rig.mesh.tetrahedra.size()
              << " volume_excess=" << rig.diagnostics.relativeVolumeExcess << " gap_bound=" << rig.diagnostics.maximumSurfaceDistance
              << " min_quality=" << rig.diagnostics.minimumTetQuality << " binding_error=" << rig.diagnostics.maximumBindingPositionError << '\n';
    const char *exportPath = std::getenv("SKY_SOFTBODY_SUPPLIED_EXPORT");
    const char *mapPath    = std::getenv("SKY_SOFTBODY_SUPPLIED_VERTEX_MAP");
    if (!exportPath && !mapPath)
        return;
    ASSERT_NE(exportPath, nullptr);
    ASSERT_NE(mapPath, nullptr);
    const auto          originalText = readText(exportPath), mapText = readText(mapPath);
    rapidjson::Document original, mapping;
    original.Parse(originalText.data(), originalText.size());
    mapping.Parse(mapText.data(), mapText.size());
    ASSERT_FALSE(original.HasParseError());
    ASSERT_FALSE(mapping.HasParseError());
    ASSERT_TRUE(original.HasMember("vertices"));
    ASSERT_TRUE(mapping.HasMember("primitives"));
    const auto &vertices = original["vertices"];
    ASSERT_TRUE(vertices.IsArray());
    const auto &body = mapping["primitives"][0];
    ASSERT_TRUE(body.HasMember("source_to_render"));
    const std::string key   = body["primitive_key"].GetString();
    const auto        bound = std::find_if(rig.binding.primitives.begin(), rig.binding.primitives.end(), [&](const auto &p) { return p.key == key; });
    ASSERT_NE(bound, rig.binding.primitives.end());
    double maximumWeightError = 0.;
    size_t compared           = 0;
    for (const auto &entry : body["source_to_render"].GetArray()) {
        const uint32_t sourceId = entry["source_vertex_id"].GetUint();
        ASSERT_LT(sourceId, vertices.Size());
        ASSERT_EQ(vertices[sourceId]["id"].GetUint(), sourceId);
        std::array<double, 16> expected{};
        for (const auto &weight : vertices[sourceId]["weights"].GetArray()) {
            ASSERT_LT(weight[0].GetUint(), expected.size());
            expected[weight[0].GetUint()] += weight[1].GetDouble();
        }
        for (const auto &index : entry["render_vertices"].GetArray()) {
            ASSERT_LT(index.GetUint(), bound->vertices.size());
            const auto            &weights = bound->vertices[index.GetUint()];
            std::array<double, 16> actual{};
            for (size_t k = 0; k < 4; ++k) {
                ASSERT_LT(weights.bones[k], actual.size());
                actual[weights.bones[k]] += weights.weights[k];
            }
            for (size_t bone = 0; bone < actual.size(); ++bone)
                maximumWeightError = std::max(maximumWeightError, std::abs(actual[bone] - expected[bone]));
            ++compared;
        }
    }
    EXPECT_EQ(compared, bound->vertices.size());
    EXPECT_LT(maximumWeightError, 2. / 65535.);
    std::cout << " original_weight_max_error=" << maximumWeightError << " compared_render_vertices=" << compared << '\n';
}

TEST(SoftBodyBuilder, SuppliedBenchmarkMatrixUsesProductionBindingPhysicsAndCompiler)
{
    const char *glbPath    = std::getenv("SKY_SOFTBODY_SUPPLIED_GLB");
    const char *matrixPath = std::getenv("SKY_SOFTBODY_SUPPLIED_MATRIX_DIR");
    if (!glbPath || !matrixPath)
        GTEST_SKIP() << "Optional external supplied benchmark matrix";
    const auto readText = [](const std::filesystem::path &path) {
        std::ifstream stream(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    };
    const auto                            compiler = aurora::cook::CreateMeshAuthoringCompiler();
    const auto                            bytes    = readText(glbPath);
    std::vector<phy::AuthoringMeshSource> sources;
    const auto                            loaded = compiler->Load({reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size()}, ".glb", sources);
    ASSERT_TRUE(loaded) << loaded.message;
    for (const uint32_t count : {30u, 25u, 20u, 15u, 10u}) {
        SCOPED_TRACE(count);
        SoftBodyRecipe recipe;
        const auto     parsed =
            ParseSoftBodyRecipe(readText(std::filesystem::path(matrixPath) / ("rabbit-" + std::to_string(count) + ".softbodyrig")), recipe);
        ASSERT_TRUE(parsed) << parsed.message;
        ASSERT_EQ(recipe.settings.generator.mode, RigGenerationMode::SuppliedCenterFan);
        ASSERT_EQ(recipe.settings.normalPolicy, SoftBodyNormalPolicy::StandardSkinning);
        ASSERT_EQ(recipe.controls.size(), count);
        EXPECT_FLOAT_EQ(recipe.settings.generator.bindingPositionScale, .7f);
        EXPECT_NEAR(recipe.settings.material.particleMass * count, 15.f, 1e-5f);
        EXPECT_NEAR(recipe.settings.material.followCompliance * 15.f / count, .02f, 1e-7f);
        EXPECT_FLOAT_EQ(recipe.settings.material.edgeCompliance, .002f);
        EXPECT_FLOAT_EQ(recipe.settings.material.volumeCompliance, 2e-16f);
        EXPECT_FLOAT_EQ(recipe.settings.material.linearDamping, 2.f);
        EXPECT_FLOAT_EQ(recipe.settings.material.vertexRadius, .02f);
        EXPECT_EQ(recipe.settings.material.solverIterations, 4u);
        ASSERT_EQ(recipe.settings.materialSlots.size(), 3u);
        const auto source = std::find_if(sources.begin(), sources.end(), [&](const auto &s) { return s.key == recipe.sourceKey; });
        ASSERT_NE(source, sources.end());
        ASSERT_EQ(source->primitives.size(), 5u);
        RigGenerationInput  input{*source, recipe.settings.bodyPrimitive, recipe.controls, recipe.settings.attachments, recipe.settings.generator};
        RigGenerationOutput rig;
        const auto          generated = GenerateRig(input, rig);
        ASSERT_TRUE(generated) << generated.message;
        ASSERT_EQ(rig.mesh.vertices.size(), count);
        ASSERT_EQ(rig.binding.bones.size(), count + 1u);
        ASSERT_TRUE(rig.diagnostics.coverageInBindingSpace);
        std::set<uint32_t> boundary;
        for (const auto &face : rig.mesh.faces)
            boundary.insert(face.begin(), face.end());
        EXPECT_EQ(boundary.size(), count - 1u);
        EXPECT_FALSE(boundary.contains(recipe.settings.generator.centerIndex));
        phy::SoftBodyDesc body;
        const auto        configured = ConfigureRigPhysics(recipe.settings.material, rig.mesh, body);
        ASSERT_TRUE(configured) << configured.message;
        ASSERT_EQ(body.rootFollow.size(), count);
        double mass            = 0.;
        double followStiffness = 0.;
        for (const auto &point : body.mesh.vertices) {
            ASSERT_EQ(point.mode, phy::SoftBodyAttachmentMode::Dynamic);
            ASSERT_GT(point.inverseMass, 0.f);
            mass += 1. / point.inverseMass;
        }
        for (const auto &follow : body.rootFollow)
            followStiffness += 1. / follow.compliance;
        EXPECT_NEAR(mass, 15., 1e-5);
        EXPECT_NEAR(followStiffness, 750., 1e-4);
        size_t   vertexCount       = 0;
        uint32_t maximumInfluences = 0;
        for (const auto &primitive : rig.binding.primitives) {
            const auto inputPrimitive =
                std::find_if(source->primitives.begin(), source->primitives.end(), [&](const auto &p) { return p.key == primitive.key; });
            ASSERT_NE(inputPrimitive, source->primitives.end());
            ASSERT_EQ(primitive.vertices.size(), inputPrimitive->positions.size());
            const bool attachment = primitive.key != recipe.settings.bodyPrimitive;
            for (const auto &weights : primitive.vertices) {
                double   sum        = 0.;
                uint32_t influences = 0;
                for (size_t slot = 0; slot < weights.weights.size(); ++slot) {
                    EXPECT_TRUE(std::isfinite(weights.weights[slot]));
                    EXPECT_GE(weights.weights[slot], 0.f);
                    EXPECT_LT(weights.bones[slot], rig.binding.bones.size());
                    influences += weights.weights[slot] > 0.f ? 1u : 0u;
                    sum += weights.weights[slot];
                }
                EXPECT_NEAR(sum, 1., 2e-7);
                maximumInfluences = std::max(maximumInfluences, influences);
                if (attachment) {
                    EXPECT_EQ(weights.bones, primitive.vertices.front().bones);
                    EXPECT_EQ(weights.weights, primitive.vertices.front().weights);
                }
            }
            vertexCount += primitive.vertices.size();
        }
        EXPECT_LE(maximumInfluences, 4u);
        rig.binding.materialSlots = recipe.settings.materialSlots;
        phy::AuthoringMeshProducts compiled;
        const auto cooked = compiler->Compile(*source, rig.binding, {Uuid::Create(), Uuid::Create(), Uuid::Create(), Uuid::Create()}, compiled);
        ASSERT_TRUE(cooked) << cooked.message;
        ASSERT_EQ(compiled.products.size(), 3u);
        const auto mesh = std::find_if(compiled.products.begin(), compiled.products.end(),
                                       [](const auto &asset) { return asset->GetType() == Name("AuroraMesh"); });
        ASSERT_NE(mesh, compiled.products.end());
        const auto &meshData = std::static_pointer_cast<Asset<aurora::Mesh>>(*mesh)->Data();
        EXPECT_EQ(meshData.materials, recipe.settings.materialSlots);
        EXPECT_EQ(meshData.subMeshes.size(), source->primitives.size());
        for (size_t index = 0; index < meshData.subMeshes.size(); ++index)
            EXPECT_EQ(meshData.subMeshes[index].materialIndex, source->primitives[index].materialIndex);
        rapidjson::StringBuffer                    buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        writer.StartObject();
        const auto integer = [&](const char *key, uint64_t value) {
            writer.Key(key);
            writer.Uint64(value);
        };
        const auto number = [&](const char *key, double value) {
            writer.Key(key);
            writer.Double(value);
        };
        integer("controlPoints", count);
        integer("actualPoints", rig.mesh.vertices.size());
        integer("surfacePoints", boundary.size());
        integer("internalPoints", rig.mesh.vertices.size() - boundary.size());
        integer("bones", rig.binding.bones.size());
        integer("edges", rig.mesh.edges.size());
        integer("tetrahedra", rig.mesh.tetrahedra.size());
        integer("boundaryTriangles", rig.mesh.faces.size());
        integer("renderVertices", vertexCount);
        integer("maxInfluences", maximumInfluences);
        integer("normalPolicy", static_cast<uint32_t>(recipe.settings.normalPolicy));
        number("sourceVolumeM3", rig.diagnostics.sourceVolume);
        number("cageVolumeM3", rig.diagnostics.cageVolume);
        number("relativeVolumeExcess", rig.diagnostics.relativeVolumeExcess);
        number("sampledDistanceM", rig.diagnostics.maximumSampledSurfaceDistance);
        number("certifiedDistanceM", rig.diagnostics.maximumSurfaceDistance);
        number("minTetQuality", rig.diagnostics.minimumTetQuality);
        number("maxRestErrorM", rig.diagnostics.maximumRestReconstructionError);
        number("maxBindingPositionErrorM", rig.diagnostics.maximumBindingPositionError);
        number("totalMassKg", mass);
        number("aggregateFollowStiffness", followStiffness);
        writer.Key("axisSupportGapsM");
        writer.StartArray();
        for (const double gap : rig.diagnostics.axisSupportGaps)
            writer.Double(gap);
        writer.EndArray();
        writer.Key("axisSupportOrder");
        writer.StartArray();
        for (const char *axis : {"-X", "+X", "-Y", "+Y", "-Z", "+Z"})
            writer.String(axis);
        writer.EndArray();
        writer.Key("nodes");
        writer.StartArray();
        for (const auto &point : rig.mesh.vertices) {
            writer.StartArray();
            writer.Double(point.position.x);
            writer.Double(point.position.y);
            writer.Double(point.position.z);
            writer.EndArray();
        }
        writer.EndArray();
        writer.Key("tets");
        writer.StartArray();
        for (const auto &tet : rig.mesh.tetrahedra) {
            writer.StartArray();
            for (const uint32_t index : tet.vertices)
                writer.Uint(index);
            writer.EndArray();
        }
        writer.EndArray();
        writer.Key("faces");
        writer.StartArray();
        for (const auto &face : rig.mesh.faces) {
            writer.StartArray();
            for (const uint32_t index : face)
                writer.Uint(index);
            writer.EndArray();
        }
        writer.EndArray();
        writer.EndObject();
        ASSERT_TRUE(writer.IsComplete());
        std::ofstream report(std::filesystem::path(matrixPath) / ("production-" + std::to_string(count) + ".json"), std::ios::binary);
        ASSERT_TRUE(report.is_open());
        report.write(buffer.GetString(), static_cast<std::streamsize>(buffer.GetSize()));
        report.flush();
        ASSERT_TRUE(report.good());
        std::cout << " supplied_matrix controls=" << rig.mesh.vertices.size() << " tets=" << rig.mesh.tetrahedra.size()
                  << " volume_excess=" << rig.diagnostics.relativeVolumeExcess << " gap_bound=" << rig.diagnostics.maximumSurfaceDistance
                  << " min_quality=" << rig.diagnostics.minimumTetQuality << " binding_error=" << rig.diagnostics.maximumBindingPositionError << '\n';
    }
}

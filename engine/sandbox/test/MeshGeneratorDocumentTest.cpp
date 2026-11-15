#include <aurora/adaptor/assets/SceneAssets.h>
#include <aurora/adaptor/components/SkeletalMeshComponent.h>
#include <aurora/adaptor/components/StaticMeshComponent.h>
#include <aurora/editor/AuroraEditorExtension.h>
#include <aurora/editor/MeshCreationSettings.h>
#include <aurora/editor/MeshGeneratorDocument.h>
#include <editor/core/actor/EditorActorCreation.h>
#include <editor/core/asset/AssetCookService.h>
#include <editor/core/asset/AssetPreviewProvider.h>
#include <editor/core/command/CommandService.h>
#include <editor/core/preferences/PreferencesPersistence.h>
#include <editor/core/preferences/PreferencesSession.h>
#include <editor/core/preferences/ProjectEditorSettings.h>
#include <editor/ui/UiTheme.h>
#include <editor/widgets/AssetReflectedWidget.h>
#include <editor/widgets/ReflectedFormView.h>
#include <framework/application/ModuleManager.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetManager.h>
#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>
#include <ui/UIPaintContext.h>
#include <ui/text/UITextSystem.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <thread>

using namespace sky;
using namespace sky::editor;
using namespace sky::aurora::editor;

namespace {
    class MaterialPreviewHost final : public IAssetPreviewProvider {
    public:
        MaterialPreviewHost() : previous(AssetPreviewProviderRegistry::Get()->GetProvider())
        {
            AssetPreviewProviderRegistry::Get()->SetProvider(this);
        }
        ~MaterialPreviewHost() override
        {
            AssetPreviewProviderRegistry::Get()->SetProvider(previous);
        }
        AssetPreviewResult CreateSession(AssetPreviewScene scene) override
        {
            input  = std::move(scene);
            handle = {Uuid::Create(), Uuid::Create()};
            ++created;
            return handle;
        }
        AssetPreviewStatusResult Status(AssetPreviewHandle id) const override
        {
            EXPECT_EQ(id, handle);
            return AssetPreviewStatus{AssetPreviewState::Ready, texture};
        }
        AssetPreviewStatusResult Update(AssetPreviewHandle id, EditorCamera camera) override
        {
            input.camera = camera;
            return Status(id);
        }
        AssetPreviewStatusResult Resize(AssetPreviewHandle id, uint32_t width, uint32_t height) override
        {
            input.width  = width;
            input.height = height;
            return Status(id);
        }
        AssetPreviewStatusResult CloseSession(AssetPreviewHandle id) override
        {
            EXPECT_EQ(id, handle);
            ++closed;
            input.world.Reset(nullptr);
            return AssetPreviewStatus{AssetPreviewState::Closed};
        }

        static constexpr ui::UITextureId texture = 23;
        AssetPreviewScene                input;
        uint32_t                         created = 0, closed = 0;

    private:
        IAssetPreviewProvider *previous;
        AssetPreviewHandle     handle;
    };

    class PreviewTextures final : public ui::IUITextureRegistry {
    public:
        ui::UITextureId RegisterTexture(const ui::UIImageData &image) override
        {
            return RegisterTextureAs(nextId++, image);
        }
        ui::UITextureId RegisterTextureAs(ui::UITextureId id, const ui::UIImageData &image) override
        {
            images[id] = image;
            return id;
        }
        void UpdateTexture(ui::UITextureId id, const ui::UIImageData &image) override
        {
            ASSERT_TRUE(images.contains(id));
            images[id] = image;
        }
        void ReleaseTexture(ui::UITextureId id) override
        {
            images.erase(id);
        }
        std::map<ui::UITextureId, ui::UIImageData> images;

    private:
        ui::UITextureId nextId = 1;
    };

    struct MeshTestSettings {
        uint32_t version = 1;
    };
    class MeshDocumentEnvironment final : public testing::Environment {
    public:
        void SetUp() override
        {
            SerializationContext::Get()->Register<MeshTestSettings>("MeshDocumentTestSettings").Member<&MeshTestSettings::version>("version");
            modules = std::make_unique<ModuleManager>();
            ASSERT_TRUE(modules->RegisterModule({"Aurora.Render", {"Aurora.Metadata"}}));
            ASSERT_TRUE(modules->RegisterModule({"Aurora.Render.Editor", {"Aurora.Metadata", "Aurora.Render"}}));
            ASSERT_TRUE(modules->TopoSort());
            StartArguments args;
            args.args = {"EditorShellTest", "--aurora-provider-only"};
            ASSERT_TRUE(modules->LoadModules(args));
            ASSERT_TRUE(modules->StartModules());
        }
        void TearDown() override
        {
            modules.reset();
        }

    private:
        std::unique_ptr<ModuleManager> modules;
    };
    const auto *meshMetadata = testing::AddGlobalTestEnvironment(new MeshDocumentEnvironment());

    class MeshTestCompiler final : public phy::IMeshAuthoringCompiler {
    public:
        phy::PhysicsStatus Load(std::span<const uint8_t>, const std::string &, std::vector<phy::AuthoringMeshSource> &out) const override
        {
            if (!primitives.empty()) {
                out = {{"seal", {}, primitives}};
                return {};
            }
            phy::AuthoringMeshPrimitive primitive;
            primitive.key       = "body";
            primitive.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
            primitive.indices   = {0, 2, 1, 0, 1, 3, 0, 3, 2, 1, 2, 3};
            for (uint32_t level = 0; level < subdivisions; ++level) {
                std::vector<uint32_t> indices;
                for (size_t i = 0; i < primitive.indices.size(); i += 3) {
                    const uint32_t a = primitive.indices[i], b = primitive.indices[i + 1], c = primitive.indices[i + 2];
                    const auto     ab = static_cast<uint32_t>(primitive.positions.size());
                    primitive.positions.push_back((primitive.positions[a] + primitive.positions[b]) * 0.5f);
                    const auto bc = static_cast<uint32_t>(primitive.positions.size());
                    primitive.positions.push_back((primitive.positions[b] + primitive.positions[c]) * 0.5f);
                    const auto ca = static_cast<uint32_t>(primitive.positions.size());
                    primitive.positions.push_back((primitive.positions[c] + primitive.positions[a]) * 0.5f);
                    indices.insert(indices.end(), {a, ab, ca, ab, b, bc, ca, bc, c, ab, bc, ca});
                }
                primitive.indices = std::move(indices);
            }
            out = {{"seal", {}, {}}};
            for (uint32_t i = 0; i < primitiveCount; ++i) {
                primitive.key            = "body" + std::to_string(i);
                primitive.transform[3].x = static_cast<float>(i) * 1.5f;
                out[0].primitives.push_back(primitive);
            }
            return {};
        }
        phy::PhysicsStatus
        ResolveMaterials(const std::string &, uint32_t count, std::span<const Uuid> supplied, std::vector<Uuid> &out) const override
        {
            out.assign(supplied.begin(), supplied.end());
            out.resize(count, Uuid::CreateWithSeed(8103));
            return {};
        }
        phy::PhysicsStatus Compile(const phy::AuthoringMeshSource &,
                                   const phy::AuthoringMeshBinding &,
                                   const phy::AuthoringProductIds &,
                                   phy::AuthoringMeshProducts &) const override
        {
            return {phy::PhysicsError::Unsupported, "Document test does not cook products"};
        }
        uint32_t                                 subdivisions   = 0;
        uint32_t                                 primitiveCount = 1;
        std::vector<phy::AuthoringMeshPrimitive> primitives;
    };
    class MeshTestOutput final : public IAssetGeneratorPayload, public IMeshGeneratorOutput {
    public:
        const void *QueryInterface(const Uuid &id) const override
        {
            return id == MESH_GENERATOR_OUTPUT_INTERFACE ? static_cast<const IMeshGeneratorOutput *>(this) : nullptr;
        }
        const MeshGeneratorOutputData &GetMeshOutput() const override
        {
            return value;
        }
        MeshGeneratorOutputData value;
    };
    class MeshTestGenerator final : public IAssetGenerator {
    public:
        explicit MeshTestGenerator(std::string id) : id(std::move(id))
        {
        }
        AssetGeneratorDescriptor Describe() const override
        {
            return {id, id, {"AuroraMesh"}, {"mesh-binding"}, GetTypeNode(TypeInfo<MeshTestSettings>::RegisteredId())};
        }
        AssetGeneratorStatus CreateSettings(Any &out) const override
        {
            out = Any(MeshTestSettings{});
            return {};
        }
        AssetGeneratorStatus CheckSupport(const AssetGeneratorContext &) const override
        {
            ++supportChecks;
            return {};
        }
        AssetGenerationOutput Generate(const AssetGenerationInput &input, AssetGenerationControl &control) const override
        {
            entered = true;
            while (blocked && !control.IsCancellationRequested()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            if (control.IsCancellationRequested()) {
                return {{AssetGeneratorCode::Cancelled, "Test generation cancelled"}, {}};
            }
            const auto *meshInput = static_cast<const IMeshGeneratorInput *>(input.payload->QueryInterface(MESH_GENERATOR_INPUT_INTERFACE));
            if (!meshInput)
                return {{AssetGeneratorCode::InvalidArgument, "Missing mesh protocol"}, {}};
            const auto &mesh              = meshInput->GetMeshInput();
            auto        result            = std::make_shared<MeshTestOutput>();
            auto       &value             = result->value;
            value.sourceOwner             = mesh.sourceOwner;
            value.sourceKey               = mesh.sourceKey;
            value.controls                = mesh.controls;
            value.binding.layoutSignature = 7;
            value.binding.bones.push_back({"root", UINT16_MAX, {}});
            value.binding.bones.push_back({"tip", 0, {}});
            value.binding.bones.back().reference.translation = {0.f, 0.8f, 0.f};
            for (const auto &primitive : mesh.mesh.primitives) {
                phy::AuthoringPrimitiveBinding binding;
                binding.key = primitive.key;
                binding.vertices.resize(primitive.positions.size());
                for (size_t i = 0; i < binding.vertices.size(); ++i) {
                    const float weight  = primitive.colors.empty() ? 0.f : primitive.colors[i].x;
                    binding.vertices[i] = {{0, 1, 0, 0}, {1.f - weight, weight, 0.f, 0.f}};
                }
                value.binding.primitives.push_back(std::move(binding));
            }
            value.cagePoints      = mesh.mesh.primitives[0].positions;
            value.tetrahedra      = {{0, 1, 2, 3}};
            value.recipeExtension = "testrecipe";
            value.recipeText      = std::to_string(input.settings.GetAsConst<MeshTestSettings>()->version);
            value.diagnostics     = {{"Control points", "4"}};
            if (invalid)
                value.binding.primitives[0].vertices[0].weights[0] = 0.5f;
            return {{}, std::move(result)};
        }
        std::string              id;
        mutable uint32_t         supportChecks = 0;
        mutable std::atomic_bool entered       = false;
        std::atomic_bool         blocked       = false;
        std::atomic_bool         invalid       = false;
    };
    class MeshDocumentTest : public testing::Test {
    protected:
        void SetUp() override
        {
            std::error_code error;
            root = std::filesystem::temp_directory_path(error) / ("sky-mesh-document-" + Uuid::Create().ToString());
            ASSERT_FALSE(error);
            ASSERT_TRUE(root.is_absolute());
            std::filesystem::create_directories(root, error);
            ASSERT_FALSE(error);
            std::ofstream(root / "seal.obj") << "original mesh";
            AssetDataBase::Get()->Reset();
            AssetDataBase::Get()->SetEngineFs({});
            AssetDataBase::Get()->SetWorkSpaceFs(new NativeFileSystem(FilePath(root.string())));
            auto source = AssetDataBase::Get()->RegisterAsset(FilePath("seal.obj"), false);
            ASSERT_TRUE(source);
            sourceOwner = source->uuid;
            compiler    = std::make_shared<MeshTestCompiler>();
            ASSERT_TRUE(phy::MeshAuthoringCompilerRegistry::Get()->Register(compiler));
            first  = std::make_shared<MeshTestGenerator>("test.mesh.first");
            second = std::make_shared<MeshTestGenerator>("test.mesh.second");
            ASSERT_TRUE(AssetGeneratorRegistry::Get()->Register(providerOwner, first));
            ASSERT_TRUE(AssetGeneratorRegistry::Get()->Register(providerOwner, second));
        }
        void TearDown() override
        {
            MeshGeneratorRecipeRegistry::Get()->UnregisterOwner(providerOwner);
            first->blocked = second->blocked = false;
            AssetGeneratorRegistry::Get()->UnregisterOwner(providerOwner);
            EXPECT_TRUE(phy::MeshAuthoringCompilerRegistry::Get()->Unregister(compiler));
            AssetDataBase::Get()->Reset();
            AssetCookService::Get()->Drain();
            AssetDataBase::Get()->SetWorkSpaceFs({});
            AssetDataBase::Get()->SetEngineFs({});
            std::error_code error;
            if (root.is_absolute() && root.filename().string().starts_with("sky-mesh-document-"))
                std::filesystem::remove_all(root, error);
            EXPECT_FALSE(error);
        }
        std::unique_ptr<IMeshGeneratorDocument> Open(const Uuid &asset = {})
        {
            auto document = CreateMeshGeneratorDocument(asset ? asset : sourceOwner);
            document->SetActionScope("mesh-test-" + Uuid::Create().ToString());
            document->Tick(0.f);
            EXPECT_NE(document->Source(), nullptr) << document->Status();
            return document;
        }
        bool Pump(IMeshGeneratorDocument &document, const std::function<bool()> &done)
        {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
            do {
                document.Tick(0.f);
                if (done())
                    return true;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } while (std::chrono::steady_clock::now() < deadline);
            return false;
        }
        bool Generate(IMeshGeneratorDocument &document, const std::string &text)
        {
            if (!document.RequestGenerate())
                return false;
            return Pump(document, [&] { return document.Generated() && document.Generated()->recipeText == text; });
        }
        void SetPreviewOption(IMeshGeneratorDocument &document, ui::UIElement &content, const std::string &path, Any value)
        {
            ASSERT_EQ(content.GetChildren().size(), 3u);
            // The mesh document's last child is the reflected preview options form.
            auto &form = static_cast<ReflectedFormView &>(*content.GetChildren().back()).Form();
            for (auto &section : form.GetSections()) {
                for (auto &field : section.fields) {
                    if (field.path == path) {
                        ASSERT_TRUE(form.Edit(field, value, document.History()));
                        return;
                    }
                }
            }
            FAIL() << "Missing preview option " << path;
        }
        std::string Read(const char *path)
        {
            std::ifstream stream(root / path);
            return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
        }
        std::filesystem::path              root;
        Uuid                               sourceOwner, providerOwner = Uuid::Create();
        std::shared_ptr<MeshTestCompiler>  compiler;
        std::shared_ptr<MeshTestGenerator> first, second;
    };
} // namespace

TEST_F(MeshDocumentTest, GeneratorActionsRefreshOnlyOnRegistrationAndInputChanges)
{
    auto       document = Open();
    auto       content  = document->CreateContent(nullptr);
    const auto initial  = first->supportChecks;
    for (int frame = 0; frame < 20; ++frame)
        document->Tick(0.f);
    EXPECT_EQ(first->supportChecks, initial);

    const auto owner = Uuid::Create();
    auto       extra = std::make_shared<MeshTestGenerator>("test.mesh.extra");
    ASSERT_TRUE(AssetGeneratorRegistry::Get()->Register(owner, extra));
    document->Tick(0.f);
    EXPECT_EQ(first->supportChecks, initial + 1);
    EXPECT_EQ(extra->supportChecks, 1u);
    AssetGeneratorRegistry::Get()->UnregisterOwner(owner);
    document->Tick(0.f);
    EXPECT_EQ(first->supportChecks, initial + 2);
    EXPECT_EQ(extra->supportChecks, 1u);

    ASSERT_TRUE(document->SetControls({{"anchor", {0, 0, 0}, MeshControlMode::Free}}));
    document->Tick(0.f);
    EXPECT_EQ(first->supportChecks, initial + 3);
}

TEST_F(MeshDocumentTest, MultipleProvidersAndWholeResultUndoRedo)
{
    auto document = Open();
    ASSERT_NE(document->Source(), nullptr);
    ASSERT_TRUE(document->SelectGenerator(second->id));
    ASSERT_EQ(document->GeneratorId(), second->id);
    ASSERT_TRUE(document->SetControls({{"fixed", {0, 0, 0}, MeshControlMode::WorldFixed}}));
    ASSERT_TRUE(document->SetSettings(Any(MeshTestSettings{2})));
    ASSERT_TRUE(Generate(*document, "2"));
    const auto complete = *document->Generated();
    EXPECT_EQ(complete.controls, document->Source()->controls);
    EXPECT_EQ(complete.diagnostics.size(), 1u);
    ASSERT_TRUE(document->History().Undo());
    EXPECT_EQ(document->Generated(), nullptr);
    ASSERT_TRUE(document->History().Redo());
    ASSERT_NE(document->Generated(), nullptr);
    EXPECT_EQ(document->Generated()->binding.layoutSignature, complete.binding.layoutSignature);
    EXPECT_EQ(document->Generated()->cagePoints, complete.cagePoints);
    EXPECT_EQ(document->Generated()->tetrahedra, complete.tetrahedra);
    ASSERT_TRUE(document->SelectGenerator(first->id));
    EXPECT_EQ(document->Settings().GetAsConst<MeshTestSettings>()->version, 1u);
    ASSERT_TRUE(document->History().Undo());
    EXPECT_EQ(document->GeneratorId(), second->id);
    EXPECT_EQ(document->Settings().GetAsConst<MeshTestSettings>()->version, 2u);
}

TEST_F(MeshDocumentTest, SettingsEditCancelsOlderOwnedRequestWithoutApplyingIt)
{
    auto document = Open();
    ASSERT_TRUE(document->SelectGenerator(first->id));
    first->blocked = true;
    ASSERT_TRUE(document->RequestGenerate());
    ASSERT_TRUE(Pump(*document, [&] { return first->entered.load(); }));
    ASSERT_TRUE(document->SetSettings(Any(MeshTestSettings{3})));
    const auto historyCount = document->History().GetUndoCount();
    ASSERT_TRUE(Pump(*document, [&] { return document->Status().find("changed during generation") != std::string::npos; })) << document->Status();
    EXPECT_EQ(document->Generated(), nullptr);
    EXPECT_EQ(document->History().GetUndoCount(), historyCount);
    first->blocked = false;
    ASSERT_TRUE(Generate(*document, "3"));
    EXPECT_EQ(document->Settings().GetAsConst<MeshTestSettings>()->version, 3u);
}

TEST_F(MeshDocumentTest, SourceRevisionChangeAndProviderRevocationCannotApplyPendingOutput)
{
    auto document = Open();
    ASSERT_TRUE(document->SelectGenerator(first->id));
    first->blocked = true;
    ASSERT_TRUE(document->RequestGenerate());
    ASSERT_TRUE(Pump(*document, [&] { return first->entered.load(); }));
    std::ofstream(root / "seal.obj", std::ios::app) << " changed";
    document->Tick(0.f);
    AssetGeneratorRegistry::Get()->UnregisterOwner(providerOwner);
    document->Tick(0.f);
    EXPECT_EQ(document->Generated(), nullptr);
    EXPECT_FALSE(document->RequestGenerate());
}

TEST_F(MeshDocumentTest, InvalidOutputRetainsCompleteCandidateAndHistory)
{
    auto document = Open();
    ASSERT_TRUE(document->SelectGenerator(first->id));
    ASSERT_TRUE(Generate(*document, "1"));
    const auto historyCount = document->History().GetUndoCount();
    first->invalid          = true;
    ASSERT_TRUE(document->RequestGenerate());
    ASSERT_TRUE(Pump(*document, [&] { return document->Status().find("not normalized") != std::string::npos; }));
    ASSERT_NE(document->Generated(), nullptr);
    EXPECT_EQ(document->Generated()->recipeText, "1");
    EXPECT_EQ(document->Generated()->binding.primitives[0].vertices[0].weights[0], 1.f);
    EXPECT_EQ(document->History().GetUndoCount(), historyCount);
}

TEST_F(MeshDocumentTest, DeferredSavePreservesOriginalAndExternalRecipeEdits)
{
    auto document = Open();
    ASSERT_TRUE(document->SelectGenerator(first->id));
    ASSERT_TRUE(Generate(*document, "1"));
    document->SetSavePath("Project/seal.testrecipe");
    document->RequestSave();
    EXPECT_TRUE(document->IsSavePending());
    EXPECT_FALSE(document->SavedOwner());
    document->Tick(0.f);
    ASSERT_TRUE(document->SavedOwner()) << document->Status();
    EXPECT_FALSE(document->IsDirty());
    EXPECT_NE(document->SavedOwner(), sourceOwner);
    EXPECT_EQ(Read("seal.obj"), "original mesh");
    EXPECT_EQ(Read("seal.testrecipe"), "1");
    ASSERT_TRUE(document->History().Undo());
    EXPECT_TRUE(document->IsDirty());
    ASSERT_TRUE(document->History().Redo());
    EXPECT_FALSE(document->IsDirty());
    ASSERT_TRUE(document->SetSettings(Any(MeshTestSettings{2})));
    ASSERT_TRUE(Generate(*document, "2"));
    std::ofstream(root / "seal.testrecipe") << "external change";
    document->RequestSave();
    document->Tick(0.f);
    EXPECT_TRUE(document->IsDirty());
    EXPECT_EQ(Read("seal.testrecipe"), "external change");
    EXPECT_EQ(Read("seal.obj"), "original mesh");
}

TEST_F(MeshDocumentTest, ReopeningSavedRecipePopulatesBindingWithoutEditingOrSaving)
{
    struct Codec final : IMeshGeneratorRecipeCodec {
        explicit Codec(MeshGeneratorRecipeState value) : recipe(std::move(value))
        {
        }
        AssetGeneratorStatus Decode(std::span<const uint8_t> bytes, MeshGeneratorRecipeState &out) const override
        {
            if (bytes.size() != 1 || bytes[0] != '2')
                return {AssetGeneratorCode::InvalidArgument, "Unexpected test recipe"};
            out = recipe;
            return {};
        }
        MeshGeneratorRecipeState recipe;
    };
    std::ofstream(root / "saved.testrecipe") << "2";
    const auto saved = AssetDataBase::Get()->RegisterAsset(FilePath("saved.testrecipe"), false);
    ASSERT_TRUE(saved);
    const MeshGeneratorRecipeState recipe{sourceOwner, "seal", first->id, {{"anchor", {}, MeshControlMode::Free}}, Any(MeshTestSettings{2})};
    ASSERT_TRUE(MeshGeneratorRecipeRegistry::Get()->Register("testrecipe", providerOwner, std::make_shared<Codec>(recipe)));
    auto document = Open(saved->uuid);
    ASSERT_TRUE(Pump(*document, [&] { return document->Generated() != nullptr; })) << document->Status();
    EXPECT_EQ(document->Generated()->recipeText, "2");
    EXPECT_EQ(document->Generated()->controls, recipe.controls);
    EXPECT_FALSE(document->IsDirty());
    EXPECT_FALSE(document->IsSavePending());
    EXPECT_EQ(document->History().GetUndoCount(), 0u);
    EXPECT_EQ(Read("saved.testrecipe"), "2");
}

TEST_F(MeshDocumentTest, MaterialViewUsesSharedCameraForOverlaysAndSwitchesBackFromWeights)
{
    const std::array<GeneratedAssetDescriptor, 3> children{{{"mesh", "AuroraMesh"}, {"skin", "AuroraSkin"}, {"skeleton", "AuroraSkeleton"}}};
    GeneratedAssetManifest                        manifest;
    ASSERT_TRUE(AssetDataBase::Get()->UpdateGeneratedProducts(sourceOwner, children, {{"mesh", "mesh"}, {"skin", "skin"}, {"skeleton", "skeleton"}},
                                                              manifest));
    std::map<std::string, Uuid> products;
    ASSERT_TRUE(AssetDataBase::Get()->GetGeneratedDefaults(sourceOwner, products));
    ASSERT_TRUE(products.contains("mesh") && products.contains("skin") && products.contains("skeleton"));

    MaterialPreviewHost host;
    auto                document = Open(products.at("mesh"));
    const Vector3       anchor{0.2f, 0.7f, 0.4f};
    ASSERT_TRUE(document->SelectGenerator(first->id));
    ASSERT_TRUE(document->SetControls({{"anchor", anchor, MeshControlMode::Free}}));
    ASSERT_TRUE(Generate(*document, "1"));
    PreviewTextures  textures;
    ui::UITextSystem text(nullptr, &textures);
    auto             content = document->CreateContent(nullptr);
    auto             preview = document->CreatePreview(&text);
    content->Arrange({650, 0, 1000, 550});
    preview->Arrange({10, 20, 650, 550});
    const auto paint = [&] {
        document->Tick(0.f);
        ui::UIPaintContext ctx;
        ctx.Begin({0, 0, 1000, 600});
        preview->Paint(ctx);
        return ctx.GetDrawData();
    };
    const auto verifyMaterial = [&] {
        const auto data = paint();
        ASSERT_GE(host.created, 1u);
        ASSERT_TRUE(host.input.world);
        const auto quad = std::find_if(data.commands.begin(), data.commands.end(), [&](const auto &c) { return c.textureId == host.texture; });
        ASSERT_NE(quad, data.commands.end());
        ASSERT_EQ(quad->indexCount, 6u);
        const auto &origin = data.vertices[data.indices[quad->indexOffset]];
        Vector2     screen;
        ASSERT_TRUE(host.input.camera.Project(anchor, {float(host.input.width), float(host.input.height)}, screen));
        bool foundControl = false, foundBone = false;
        for (auto item = std::next(quad); item != data.commands.end(); ++item) {
            for (uint32_t i = item->indexOffset; i < item->indexOffset + item->indexCount; ++i) {
                const auto &vertex = data.vertices[data.indices[i]];
                if (item->shape && std::abs(vertex.roundCenterX - origin.x - screen.x) < 1.f &&
                    std::abs(vertex.roundCenterY - origin.y - screen.y) < 1.f)
                    foundControl = true;
                if (!item->shape && item->textureId == ui::UI_INVALID_TEXTURE && vertex.color == GetDefaultUiTheme().colors.accent)
                    foundBone = true;
            }
        }
        EXPECT_TRUE(foundControl) << "The control overlay must follow the material view's camera and viewport";
        EXPECT_TRUE(foundBone) << "The generated deformation bone must overlay the material view";
    };
    ASSERT_NO_FATAL_FAILURE(verifyMaterial());
    const aurora::SkeletalMeshComponent *model = nullptr;
    for (const auto &actor : host.input.world->GetActors()) {
        if (const auto *component = actor->GetComponent<aurora::SkeletalMeshComponent>()) {
            ASSERT_EQ(model, nullptr);
            model = component;
        }
    }
    ASSERT_NE(model, nullptr);
    EXPECT_EQ(model->GetMeshUuid(), products.at("mesh"));
    EXPECT_EQ(model->GetSkinUuid(), products.at("skin"));
    EXPECT_EQ(model->GetSkeletonUuid(), products.at("skeleton"));
    EXPECT_EQ(model->GetPoseMode(), aurora::SkeletalPoseMode::ReferencePose);
    EXPECT_FALSE(model->GetMaterialUuid());
    const auto firstSize = host.input.width;
    preview->Arrange({15, 20, 570, 440});
    ASSERT_NO_FATAL_FAILURE(verifyMaterial());
    EXPECT_NE(host.input.width, firstSize);
    const auto         initialPosition = host.input.camera.position;
    ui::UIPointerEvent dolly;
    dolly.action     = ui::UIPointerAction::WHEEL;
    dolly.x          = 250.f;
    dolly.y          = 200.f;
    dolly.wheelDelta = 1.f;
    EXPECT_EQ(preview->OnPointerEvent(dolly), ui::UIEventResult::HANDLED);
    ASSERT_NO_FATAL_FAILURE(verifyMaterial());
    EXPECT_NE(host.input.camera.position, initialPosition);
    EXPECT_TRUE(textures.images.empty());
    ASSERT_NO_FATAL_FAILURE(SetPreviewOption(*document, *content, "showWeights", Any(true)));
    const auto heat = paint();
    EXPECT_TRUE(std::any_of(heat.commands.begin(), heat.commands.end(), [&](const auto &cmd) { return cmd.textureId == host.texture; }));
    ASSERT_TRUE(std::any_of(heat.commands.begin(), heat.commands.end(), [&](const auto &cmd) { return textures.images.contains(cmd.textureId); }));
    ASSERT_EQ(textures.images.size(), 1u);
    const auto fullWidth = textures.images.begin()->second.width;
    ASSERT_GT(fullWidth, 384u);
    EXPECT_EQ(preview->OnPointerEvent(dolly), ui::UIEventResult::HANDLED);
    paint();
    EXPECT_EQ(textures.images.begin()->second.width, 384u);
    ASSERT_TRUE(Pump(*document, [&] {
        paint();
        return textures.images.begin()->second.width == fullWidth;
    }));
    ASSERT_NO_FATAL_FAILURE(SetPreviewOption(*document, *content, "showWeights", Any(false)));
    ASSERT_NO_FATAL_FAILURE(verifyMaterial());
    EXPECT_EQ(preview->OnPointerEvent(dolly), ui::UIEventResult::HANDLED);
    ASSERT_NO_FATAL_FAILURE(verifyMaterial());
    ASSERT_NO_FATAL_FAILURE(SetPreviewOption(*document, *content, "showWeights", Any(true)));
    paint();
    ASSERT_EQ(textures.images.size(), 1u);
    EXPECT_EQ(textures.images.begin()->second.width, fullWidth);
    preview.reset();
    EXPECT_EQ(host.created, 1u);
    EXPECT_EQ(host.closed, host.created);
}

TEST_F(MeshDocumentTest, HighResolutionWeightsPreserveCoverageWithinUiBudget)
{
    compiler->subdivisions   = 6;
    compiler->primitiveCount = 3;
    auto document            = Open();
    ASSERT_NE(document->Source(), nullptr);
    ASSERT_TRUE(document->SelectGenerator(first->id));
    ASSERT_TRUE(document->SetControls({{"anchor", {0, 0, 0}, MeshControlMode::Free}}));
    ASSERT_TRUE(Generate(*document, "1"));
    const auto original      = document->Source()->mesh;
    size_t     triangleCount = 0;
    for (const auto &primitive : original.primitives)
        triangleCount += primitive.indices.size() / 3;
    ASSERT_GT(triangleCount * 3, 65536u);

    PreviewTextures  textures;
    ui::UITextSystem text(nullptr, &textures);
    auto             content = document->CreateContent(nullptr);
    auto             preview = document->CreatePreview(&text);
    ASSERT_NE(content, nullptr);
    ASSERT_NE(preview, nullptr);
    content->Arrange({900, 0, 1280, 900});
    preview->Arrange({0, 0, 900, 900});
    ASSERT_NO_FATAL_FAILURE(SetPreviewOption(*document, *content, "showWeights", Any(true)));
    ui::UIImageData firstBoneImage;
    for (uint32_t bone : {0u, 1u}) {
        ASSERT_NO_FATAL_FAILURE(SetPreviewOption(*document, *content, "weightBone", Any(bone)));
        ui::UIPaintContext paint;
        paint.Begin({0, 0, 1280, 900});
        content->Paint(paint);
        preview->Paint(paint);
        const auto &data = paint.GetDrawData();
        EXPECT_LT(data.vertices.size(), 65536u - 16384u);
        EXPECT_LT(data.indices.size(), 196608u - 16384u);
        EXPECT_LT(data.commands.size(), 4096u - 256u);
        EXPECT_TRUE(std::all_of(data.indices.begin(), data.indices.end(), [&](uint32_t i) { return i < data.vertices.size(); }));
        EXPECT_TRUE(std::all_of(data.vertices.begin(), data.vertices.end(), [](const auto &vertex) {
            return std::isfinite(vertex.x) && std::isfinite(vertex.y) && std::abs(vertex.x / 1280.f) <= 16.f && std::abs(vertex.y / 900.f) <= 16.f;
        }));
        const auto textured = std::find_if(data.commands.begin(), data.commands.end(), [&](const auto &command) {
            return command.indexCount == 6 && textures.images.contains(command.textureId);
        });
        ASSERT_NE(textured, data.commands.end());
        const auto &image = textures.images.at(textured->textureId);
        ASSERT_GT(image.width, 0u);
        ASSERT_GT(image.height, 0u);
        ASSERT_EQ(image.pixels.size(), size_t{image.width} * image.height * 4u);
        EXPECT_EQ(image.pixels[3], 0u);
        // Each tetrahedron is convex and they do not overlap. Each scanline must intersect three uninterrupted spans.
        for (uint32_t row : {image.height * 48u / 100u, image.height / 2u, image.height * 52u / 100u}) {
            std::vector<uint32_t> spans;
            uint32_t              length = 0;
            for (uint32_t x = 0; x < image.width; ++x) {
                if (image.pixels[(size_t{row} * image.width + x) * 4u + 3u] != 0) {
                    ++length;
                } else if (length != 0u) {
                    spans.push_back(length);
                    length = 0;
                }
            }
            EXPECT_EQ(length, 0u);
            ASSERT_EQ(spans.size(), 3u) << "row " << row;
            for (const auto span : spans)
                EXPECT_GT(span, image.width / 40u);
        }
        if (bone == 0u) {
            firstBoneImage = image;
            continue;
        }
        ASSERT_EQ(firstBoneImage.width, image.width);
        ASSERT_EQ(firstBoneImage.height, image.height);
        size_t visible = 0, recolored = 0, coverageChanges = 0;
        for (size_t i = 0; i < image.pixels.size(); i += 4) {
            coverageChanges += (firstBoneImage.pixels[i + 3] != 0) != (image.pixels[i + 3] != 0);
            if (image.pixels[i + 3] != 0) {
                ++visible;
                recolored += !std::equal(image.pixels.begin() + i, image.pixels.begin() + i + 3, firstBoneImage.pixels.begin() + i);
            }
        }
        EXPECT_EQ(coverageChanges, 0u);
        EXPECT_GT(recolored, visible / 2u);
    }
    ASSERT_EQ(document->Source()->mesh.primitives.size(), original.primitives.size());
    ASSERT_EQ(document->Generated()->binding.primitives.size(), original.primitives.size());
    for (size_t i = 0; i < original.primitives.size(); ++i) {
        EXPECT_EQ(document->Source()->mesh.primitives[i].positions, original.primitives[i].positions);
        EXPECT_EQ(document->Source()->mesh.primitives[i].indices, original.primitives[i].indices);
        const auto &binding = document->Generated()->binding.primitives[i];
        ASSERT_EQ(binding.vertices.size(), original.primitives[i].positions.size());
        EXPECT_TRUE(std::all_of(binding.vertices.begin(), binding.vertices.end(), [](const auto &vertex) { return vertex.weights[0] == 1.f; }));
    }
    preview.reset();
    EXPECT_TRUE(textures.images.empty());
}

TEST_F(MeshDocumentTest, WeightPreviewShowsFrontSurfaceGradientInEitherPrimitiveOrder)
{
    phy::AuthoringMeshPrimitive front;
    front.key       = "front";
    front.positions = {{-1, -1, 0}, {1, -1, 0}, {0, 1, 0}, {0, 0, 0.25f}};
    front.indices   = {0, 1, 2};
    front.colors    = {{0, 0, 0, 1}, {1, 0, 0, 1}, {0.5f, 0, 0, 1}, {0, 0, 0, 1}};
    auto back       = front;
    back.key        = "back";
    back.colors.clear();
    // Keep the two fixture faces perpendicular to the preview camera and on the same sight line.
    const auto camera = MakeAssetPreviewCamera();
    const auto right = camera.Right(), up = camera.Up(), towardCamera = -camera.Forward();
    back.transform = {
        {right.x, right.y, right.z, 0.f}, {up.x, up.y, up.z, 0.f}, {towardCamera.x, towardCamera.y, towardCamera.z, 0.f}, {0.f, 0.f, 0.f, 1.f}};
    front.transform = back.transform;
    for (uint32_t axis = 0; axis < 3; ++axis)
        front.transform[3][axis] = towardCamera[axis] * 2.f;

    std::array<std::array<uint8_t, 3>, 3> reference;
    for (const bool reverse : {false, true}) {
        compiler->primitives = {front, back};
        if (reverse)
            std::reverse(compiler->primitives.begin(), compiler->primitives.end());
        auto document = Open();
        ASSERT_TRUE(document->SelectGenerator(first->id));
        ASSERT_TRUE(Generate(*document, "1"));
        PreviewTextures  textures;
        ui::UITextSystem text(nullptr, &textures);
        auto             content = document->CreateContent(nullptr);
        auto             preview = document->CreatePreview(&text);
        content->Arrange({600, 0, 900, 600});
        preview->Arrange({0, 0, 600, 600});
        ASSERT_NO_FATAL_FAILURE(SetPreviewOption(*document, *content, "showWeights", Any(true)));
        ui::UIPaintContext paint;
        paint.Begin({0, 0, 900, 600});
        preview->Paint(paint);
        ASSERT_EQ(textures.images.size(), 1u);
        const auto &image = textures.images.begin()->second;
        ASSERT_EQ(image.pixels.size(), size_t{image.width} * image.height * 4u);
        ASSERT_GT(image.width, 30u);
        std::array<std::array<uint8_t, 3>, 3> samples;
        for (size_t i = 0; i < samples.size(); ++i) {
            const size_t x      = image.width * (45u + 5u * i) / 100u;
            const size_t offset = ((image.height / 2u) * size_t{image.width} + x) * 4u;
            ASSERT_EQ(image.pixels[offset + 3], 255u);
            std::copy_n(image.pixels.begin() + offset, 3, samples[i].begin());
        }
        // One unsubdivided foreground face interpolates three distinct weights across the same row.
        EXPECT_NE(samples[0], samples[1]);
        EXPECT_NE(samples[1], samples[2]);
        if (reverse)
            EXPECT_EQ(samples, reference);
        else
            reference = samples;
    }
}

namespace {
    class CreationSettingsStorage final : public IPreferenceStorage {
    public:
        PreferenceResult Read(std::string &out) override
        {
            if (bytes.empty())
                return {PreferenceCode::NotFound};
            out = bytes;
            return {};
        }
        PreferenceResult Write(const std::string &value) override
        {
            bytes = value;
            return {};
        }
        std::string bytes;
    };
} // namespace

TEST(ProjectCreationSettings, ProjectApplyIsIsolatedAndCancelPreservesAppliedDefaults)
{
    MeshCreationSettings   project, user;
    PreferencePageRegistry projectPages, userPages;
    auto                  *type = GetTypeNode(TypeInfo<MeshCreationSettings>::RegisteredId());
    ASSERT_NE(type, nullptr);
    projectPages.Add({"aurora.creation", "Mesh", &project, type});
    userPages.Add({"aurora.creation", "Mesh", &user, type});
    auto                   projectFile = std::make_shared<CreationSettingsStorage>();
    auto                   userFile    = std::make_shared<CreationSettingsStorage>();
    PreferencesPersistence projectStore(projectFile), userStore(userFile);
    PreferencesSession     session;
    session.Begin(&projectPages);
    const auto *page = session.GetPage(0);
    ASSERT_NE(page, nullptr);
    const auto         asset = Uuid::Create();
    PropertyDescriptor material(page->object, &type->members.at("material"), "material", "");
    session.History().Execute(material.MakeEditCommand(Any(asset)));
    EXPECT_FALSE(project.material);
    ASSERT_TRUE(session.ApplyDetailed(&projectStore).Succeeded());
    EXPECT_EQ(project.material, asset);
    EXPECT_FALSE(user.material);
    EXPECT_TRUE(userFile->bytes.empty());
    EXPECT_FALSE(session.History().CanUndo());
    session.History().Execute(material.MakeEditCommand(Any(Uuid{})));
    session.End();
    EXPECT_EQ(project.material, asset);
    MeshCreationSettings   reopened;
    PreferencePageRegistry nextProject;
    nextProject.Add({"aurora.creation", "Mesh", &reopened, type});
    ASSERT_TRUE(projectStore.Load(nextProject).Succeeded());
    EXPECT_EQ(reopened.material, asset);
    EXPECT_EQ(userStore.Load(userPages).code, PreferenceCode::NotFound);
}

TEST(ProjectCreationSettings, InvalidAssetUuidDoesNotOverwriteLiveValuesOrFile)
{
    MeshCreationSettings settings;
    settings.material = Uuid::Create();
    PreferencePageRegistry pages;
    pages.Add({"aurora.creation", "Mesh", &settings, GetTypeNode(TypeInfo<MeshCreationSettings>::RegisteredId())});
    auto                         file = std::make_shared<CreationSettingsStorage>();
    PreferencesPersistence       store(file);
    std::vector<PreferenceValue> values;
    ASSERT_TRUE(CapturePreferenceValues(pages.GetPages(), values).Succeeded());
    ASSERT_TRUE(store.Save(values).Succeeded());
    const auto uuid   = settings.material.ToString();
    const auto offset = file->bytes.find(uuid);
    ASSERT_NE(offset, std::string::npos);
    file->bytes[offset + 8] = '_';
    const auto malformed    = file->bytes;
    const auto result       = store.Load(pages);
    EXPECT_EQ(result.code, PreferenceCode::InvalidData);
    EXPECT_EQ(settings.material.ToString(), uuid);
    EXPECT_EQ(file->bytes, malformed);
}

TEST_F(MeshDocumentTest, ProjectDefaultsAcceptCatalogMaterialAndRejectOtherAssetTypes)
{
    const std::array<GeneratedAssetDescriptor, 3> children{{{"material", "AuroraMaterial"}, {"alternate", "AuroraMaterial"}, {"mesh", "AuroraMesh"}}};
    GeneratedAssetManifest                        manifest;
    ASSERT_TRUE(AssetDataBase::Get()->UpdateGeneratedProducts(sourceOwner, children,
                                                              {{"material", "material"}, {"alternate", "alternate"}, {"mesh", "mesh"}}, manifest));
    std::map<std::string, Uuid> products;
    ASSERT_TRUE(AssetDataBase::Get()->GetGeneratedDefaults(sourceOwner, products));
    EditorAssetCatalog::Get()->Refresh();
    struct RegisteredExtension {
        AuroraEditorExtension value;
        RegisteredExtension()
        {
            value.Register();
        }
        ~RegisteredExtension()
        {
            value.Unregister();
        }
    } extension;
    auto      &pages = ProjectEditorSettings::Get()->Pages();
    const auto entry = std::ranges::find(pages.GetPages(), std::string("aurora.creation"), &PreferencePage::id);
    ASSERT_NE(entry, pages.GetPages().end());
    auto &settings    = *static_cast<MeshCreationSettings *>(entry->object);
    settings.material = products.at("material");
    ASSERT_TRUE(ValidateMeshCreationSettings(settings).Succeeded());
    std::filesystem::create_directories(root / "engine/assets/shapes");
    std::ofstream(root / "engine/assets/shapes/cube.glb") << "fixture";
    AssetDataBase::Get()->SetEngineFs(new NativeFileSystem(FilePath(root / "engine")));
    ASSERT_TRUE(AssetDataBase::Get()->RegisterAsset("engine", FilePath(sky::aurora::scene_assets::CUBE), false));
    const auto creator = EditorActorCreation::Get()->Find("cube");
    ASSERT_TRUE(creator);
    ProjectEditorSettings::Get()->SetReady(false);
    Actor blocked;
    EXPECT_FALSE(creator->create(blocked));
    EXPECT_FALSE(blocked.GetComponent<sky::aurora::StaticMeshComponent>());
    ProjectEditorSettings::Get()->SetReady(true);
    Actor actor;
    ASSERT_TRUE(creator->create(actor));
    auto *mesh = actor.GetComponent<sky::aurora::StaticMeshComponent>();
    ASSERT_TRUE(mesh);
    EXPECT_EQ(mesh->GetMaterialUuid(), settings.material);
    settings.material = products.at("alternate");
    Actor second;
    ASSERT_TRUE(creator->create(second));
    auto *secondMesh = second.GetComponent<sky::aurora::StaticMeshComponent>();
    ASSERT_TRUE(secondMesh);
    EXPECT_EQ(secondMesh->GetMaterialUuid(), products.at("alternate"));
    EXPECT_EQ(mesh->GetMaterialUuid(), products.at("material"));
    settings.material           = products.at("material");
    auto                   file = std::make_shared<CreationSettingsStorage>();
    PreferencesPersistence store(file);
    PreferencesSession     session;
    session.Begin(&pages);
    ASSERT_TRUE(session.ApplyDetailed(&store).Succeeded());
    session.End();
    settings.material = products.at("mesh");
    EXPECT_EQ(ValidateMeshCreationSettings(settings).code, PreferenceCode::ValidationFailed);
    ASSERT_TRUE(store.Load(pages).Succeeded());
    EXPECT_EQ(settings.material, products.at("material"));
}

TEST(EngineSceneAssets, LegacyIdentitiesAreRepositoryAssetsWithPathsHintsAndDependencies)
{
    ModuleManager cookModules;
    cookModules.RegisterModule({"Aurora.Cook", {"Aurora.Metadata"}});
    ASSERT_TRUE(cookModules.TopoSort());
    ASSERT_TRUE(cookModules.LoadModules({}));
    ASSERT_TRUE(cookModules.StartModules());
    using namespace sky::aurora;
    struct EngineMount {
        EngineMount()
        {
            AssetDataBase::Get()->Reset();
            AssetDataBase::Get()->SetWorkSpaceFs({});
            AssetDataBase::Get()->SetEngineFs(new NativeFileSystem(FilePath(std::filesystem::current_path())));
            AssetManager::Get()->SetSourceCatalog(AssetDataBase::Get());
        }
        ~EngineMount()
        {
            AssetManager::Get()->SetSourceCatalog(nullptr);
            AssetDataBase::Get()->Reset();
            AssetDataBase::Get()->SetEngineFs({});
        }
    } mount;
    const std::array paths{scene_assets::CUBE, scene_assets::PLANE, scene_assets::LIT, scene_assets::UNLIT};
    const std::array ids{"8F015801-3437-4A44-A581-C191184C8101", "8F015801-3437-4A44-A581-C191184C8102", "8F015801-3437-4A44-A581-C191184C8104",
                         "8F015801-3437-4A44-A581-C191184C8103"};
    for (size_t i = 0; i < paths.size(); ++i) {
        const auto source = AssetDataBase::Get()->RegisterAsset(FilePath(paths[i]), false);
        ASSERT_TRUE(source);
        EXPECT_EQ(source->uuid, Uuid::CreateFromString(ids[i]));
    }
    for (int session = 0; session < 2; ++session) {
        struct ExtensionScope {
            AuroraEditorExtension extension;
            ExtensionScope()
            {
                extension.Register();
            }
            ~ExtensionScope()
            {
                extension.Unregister();
            }
        } scope;
        StaticMeshComponent component;
        auto               *node = GetTypeNode(TypeInfo<StaticMeshComponent>::RegisteredId());
        ASSERT_TRUE(node);
        AssetReflectedWidget widget;
        for (size_t i = 0; i < paths.size(); ++i) {
            const auto  id     = Uuid::CreateFromString(ids[i]);
            const auto  path   = std::string("Engine/") + paths[i];
            const bool  mesh   = i < 2;
            const auto *member = mesh ? "mesh" : "material";
            if (mesh)
                component.SetMeshUuid(id);
            else
                component.SetMaterialUuid(id);
            EditorAssetItem item;
            ASSERT_TRUE(EditorAssetCatalog::Get()->Find(id, item));
            EXPECT_EQ(item.path, path);
            EXPECT_EQ(item.mount, "Engine");
            EXPECT_FALSE(item.writable);
            PropertyField field;
            field.descriptor        = PropertyDescriptor(&component, &node->members.at(member), member, "");
            field.control.assetType = mesh ? "AuroraMesh" : "AuroraMaterial";
            std::string hint;
            ASSERT_TRUE(widget.BuildTooltip(field, 1, hint));
            EXPECT_NE(hint.find(path), std::string::npos);
            EXPECT_NE(hint.find("GUID: " + id.ToString()), std::string::npos);
            EXPECT_EQ(hint.find("Missing asset"), std::string::npos);
            std::vector<ComponentAssetReference> dependencies;
            component.CollectAssetReferences(dependencies);
            EXPECT_TRUE(std::ranges::any_of(dependencies, [&](const auto &ref) { return ref.uuid == id; }));
            EXPECT_EQ(std::ranges::any_of(dependencies, [&](const auto &ref) { return ref.uuid == Uuid::CreateFromString(ids[3]); }), i == 3);
        }
        for (const auto &folder : EditorAssetCatalog::Get()->GetRoots())
            EXPECT_NE(folder.path, "Builtin");
    }
    std::string path;
    EXPECT_TRUE(EditorAssetCatalog::Get()->GetPath(Uuid::CreateFromString(ids[2]), path));
    EXPECT_EQ(path, std::string("Engine/") + scene_assets::LIT);
}

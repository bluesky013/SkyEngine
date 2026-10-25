#include <animation/IAnimationClipSource.h>
#include <animation/IAnimationInstanceControl.h>
#include <animation/IAnimationSkeletonSource.h>
#include <aurora/renderer/ISkeletalPoseSource.h>
#include <framework/application/ModuleManager.h>
#include <framework/asset/AssetBuilderManager.h>
#include <framework/asset/AssetManager.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/world/WorldSubSystemRegistry.h>
#include <gtest/gtest.h>
#if ANIMATION_TEST_EDITOR
#include <editor/core/asset/AssetPreviewProvider.h>
#include <editor/core/asset/AssetViewData.h>
#include <editor/core/extension/EditorExtensionHost.h>
#include <editor/core/extension/EditorExtensionRegistry.h>
#include <editor/core/extension/EditorActionRegistry.h>
#include <editor/core/extension/EditorScopeStack.h>
#include <editor/ui/AssetViewProvider.h>
#include <editor/widgets/ReflectedFormView.h>
#endif

using namespace sky;

TEST(AnimationModules, RealProvidersUnregisterAndRequestsOutliveModuleManager)
{
    struct SystemNotify final : ISystemNotify {
        void SetExit() override {}
        ModuleManager *GetModuleManager() const override { return manager; }
        ModuleManager *manager = nullptr;
    } system;
    auto manager = std::make_unique<ModuleManager>();
    system.manager = manager.get();
    Interface<ISystemNotify>::Get()->Register(system);
    manager->RegisterModule({"Content.Module", {"Content.Metadata"}});
    manager->RegisterModule({"Aurora.Render", {"Aurora.Metadata"}});
    manager->RegisterModule({"Animation.World", {"Animation.GraphAssets", "Content.Module", "Aurora.Render"}});
#if ANIMATION_TEST_EDITOR
    manager->RegisterModule({"Aurora.Render.Editor", {"Aurora.Metadata"}});
    manager->RegisterModule({"Content.Views", {"Content.Metadata"}});
    manager->RegisterModule({"Animation.Editor", {"Animation.GraphAssets", "Animation.World", "Aurora.Render.Editor", "Content.Views"}});
#endif
    manager->TopoSort();
    StartArguments arguments;
    arguments.args = {"Animation.ModuleTest", "--aurora-provider-only"};
    ASSERT_TRUE(manager->LoadModules(arguments));
    manager->StartModules();
    auto *clips = Interface<IAnimationClipSource>::Get()->GetApi();
    auto *rigs = Interface<IAnimationSkeletonSource>::Get()->GetApi();
    ASSERT_NE(clips, nullptr); ASSERT_NE(rigs, nullptr);
    ASSERT_NE(Interface<IAnimationInstanceControl>::Get()->GetApi(), nullptr);
    ASSERT_NE(Interface<aurora::ISkeletalPoseSource>::Get()->GetApi(), nullptr);
    auto clipRequest = clips->Request({});
    auto rigRequest = rigs->Request({});
    WorldPtr world = World::CreateWorld();
    WorldDesc desc;
    desc.subSystems.push_back({Name("Animation"), {}, true});
    world->Build(desc);
    ASSERT_NE(world->GetSubSystem(Name("Animation")), nullptr);
#if ANIMATION_TEST_EDITOR
    using namespace sky::editor;
    auto extensions = std::make_unique<EditorExtensionHost>();
    for (auto &entry : EditorExtensionRegistry::Get()->TakeAll()) extensions->Add(std::move(entry));
    extensions->RegisterAll();
    ASSERT_TRUE(AssetViewDataRegistry::Get()->Has("AuroraSkin"));
    ASSERT_TRUE(AssetViewDataRegistry::Get()->Has("AnimationClip"));
    extensions->UnregisterAll();
    EXPECT_FALSE(AssetViewDataRegistry::Get()->Has("AuroraSkin"));
    extensions->RegisterAll();
    ASSERT_TRUE(AssetViewDataRegistry::Get()->Has("AuroraSkin"));
    auto skinSummary = AssetViewDataRegistry::Get()->Build("AuroraSkin", Uuid::Create());
    EXPECT_TRUE(skinSummary.moduleLease);
    struct PreviewHost final : IAssetPreviewProvider {
        AssetPreviewResult CreateSession(AssetPreviewScene value) override
        {
            input = std::move(value);
            ++created;
            return AssetPreviewHandle{Uuid::Create(), Uuid::Create()};
        }
        AssetPreviewStatusResult Resize(AssetPreviewHandle, uint32_t, uint32_t) override { return AssetPreviewStatus{AssetPreviewState::Ready}; }
        AssetPreviewStatusResult Update(AssetPreviewHandle, EditorCamera) override { return AssetPreviewStatus{AssetPreviewState::Ready}; }
        AssetPreviewStatusResult Status(AssetPreviewHandle) const override { return AssetPreviewStatus{AssetPreviewState::Ready}; }
        AssetPreviewStatusResult CloseSession(AssetPreviewHandle) override { ++closed; return AssetPreviewStatus{AssetPreviewState::Closed}; }
        AssetPreviewScene input;
        int created = 0, closed = 0;
    } previewHost;
    AssetPreviewProviderRegistry::Get()->SetProvider(&previewHost);
    auto *viewProvider = AssetViewProviderRegistry::Get()->Find("AnimationClip");
    ASSERT_NE(viewProvider, nullptr);
    auto document = viewProvider->OpenDocument(Uuid::Create(), "AnimationClip");
    ASSERT_NE(document.document, nullptr);
    auto content = document.document->CreateContent(nullptr);
    auto viewport = document.document->CreatePreview(nullptr);
    ASSERT_NE(content, nullptr);
    ASSERT_NE(viewport, nullptr);
    EditorScopeStack::Get()->Push("assetViewer");
    auto oldPreviewAction = EditorActionRegistry::Get()->GetSnapshot("animation.preview.play");
    EditorScopeStack::Get()->Pop("assetViewer");
    ASSERT_TRUE(oldPreviewAction.action.moduleLease);
    content->Arrange({0, 0, 600, 800});
    viewport->Arrange({0, 0, 320, 480});
    ASSERT_EQ(content->GetChildren().size(), 1u);
    auto *form = static_cast<ReflectedFormView *>(content->GetChildren().front().get());
    const auto skeleton = Uuid::Create(), mesh = Uuid::Create(), skin = Uuid::Create(), material = Uuid::Create();
    for (const auto &[name, value] : {std::pair{"skeleton", skeleton}, std::pair{"mesh", mesh}, std::pair{"skin", skin}, std::pair{"material", material}}) {
        bool edited = false;
        for (auto &section : form->Form().GetSections())
            for (auto &field : section.fields)
                if (field.descriptor.GetName() == name) edited = form->Form().Edit(field, Any(value), document.document->History());
        ASSERT_TRUE(edited) << name;
    }
    document.document->Tick(0);
    ASSERT_EQ(previewHost.created, 1);
    ASSERT_TRUE(previewHost.input.world);
    ASSERT_EQ(previewHost.input.world->GetActors().size(), 1u);
    ASSERT_NE(previewHost.input.world->GetSubSystem(Name("Animation")), nullptr);
    auto *previewActor = previewHost.input.world->GetActors().front().get();
    const auto *meshType = SerializationContext::Get()->FindType("SkeletalMeshComponent");
    const auto *animationType = SerializationContext::Get()->FindType("AnimationComponent");
    ASSERT_NE(meshType, nullptr); ASSERT_NE(animationType, nullptr);
    auto *meshComponent = previewActor->GetComponent(meshType->info->registeredId);
    ASSERT_NE(meshComponent, nullptr);
    ASSERT_NE(previewActor->GetComponent(animationType->info->registeredId), nullptr);
    {
        const auto configuredSkin = PropertyDescriptor(meshComponent, &meshType->members.at("skin"), "skin", {}).GetValue();
        ASSERT_NE(configuredSkin.GetAsConst<Uuid>(), nullptr);
        EXPECT_EQ(*configuredSkin.GetAsConst<Uuid>(), skin);
    }
    EXPECT_NE(previewHost.input.world.Get(), world.Get());
    struct ParameterControl final : IAnimationInstanceControl {
        Uuid parameter = Uuid::Create();
        AnimationParam value = .25f;
        AnimationInstanceStatus Find(const World *world, const Uuid &actor) const override
        {
            AnimationInstanceStatus state;
            state.handle.world = world;
            state.handle.actor = actor;
            state.state = AnimationSourceState::Ready;
            state.parameters = {{parameter, "Speed", value}};
            return state;
        }
        AnimationStatus Control(const AnimationInstanceHandle &, AnimationControlOp, float) override { return {}; }
        AnimationStatus SetParameter(const AnimationInstanceHandle &, const Uuid &id, const AnimationParam &next) override
        {
            if (id != parameter || next.index() != value.index()) return {AnimationError::InvalidAsset, "parameter identity mismatch"};
            value = next;
            return {};
        }
    } parameterControl;
    auto *productionControl = Interface<IAnimationInstanceControl>::Get()->GetApi();
    Interface<IAnimationInstanceControl>::Get()->Register(parameterControl);
    document.document->Tick(0);
    document.document->Tick(0);
    PropertyField *parameterValue = nullptr, *parameterList = nullptr;
    const auto findFields = [&](auto &&visit, std::vector<PropertyField> &fields) -> void {
        for (auto &field : fields) {
            if (field.descriptor.GetName() == "floatValue") parameterValue = &field;
            if (field.descriptor.GetName() == "parameters") parameterList = &field;
            visit(visit, field.children);
        }
    };
    for (auto &section : form->Form().GetSections()) findFields(findFields, section.fields);
    ASSERT_NE(parameterValue, nullptr);
    ASSERT_NE(parameterList, nullptr);
    EXPECT_FALSE(form->Form().RemoveSequenceElement(*parameterList, 0, document.document->History()));
    ASSERT_TRUE(form->Form().Edit(*parameterValue, Any(.75f), document.document->History()));
    for (int i = 0; i < 4; ++i) document.document->Tick(0);
    EXPECT_FLOAT_EQ(std::get<float>(parameterControl.value), .75f);
    ASSERT_TRUE(document.document->History().Undo());
    document.document->Tick(0);
    EXPECT_FLOAT_EQ(std::get<float>(parameterControl.value), .25f);
    ASSERT_TRUE(document.document->History().Redo());
    document.document->Tick(0);
    EXPECT_FLOAT_EQ(std::get<float>(parameterControl.value), .75f);
    Interface<IAnimationInstanceControl>::Get()->Register(*productionControl);
#endif
    manager->UnLoadModules();
    EXPECT_EQ(Interface<IAnimationClipSource>::Get()->GetApi(), nullptr);
    EXPECT_EQ(Interface<IAnimationSkeletonSource>::Get()->GetApi(), nullptr);
    EXPECT_EQ(Interface<IAnimationInstanceControl>::Get()->GetApi(), nullptr);
    EXPECT_EQ(Interface<aurora::ISkeletalPoseSource>::Get()->GetApi(), nullptr);
    EXPECT_FALSE(WorldSubSystemRegistry::Get().IsRegistered(Name("Animation")));
    manager.reset(); system.manager = nullptr;
#if ANIMATION_TEST_EDITOR
    EXPECT_EQ(AssetViewProviderRegistry::Get()->Find("AnimationClip"), nullptr);
    EXPECT_EQ(AssetViewProviderRegistry::Get()->Find("AnimationGraph"), nullptr);
    EXPECT_FALSE(AssetViewDataRegistry::Get()->Has("AuroraSkin"));
    EXPECT_FALSE(AssetViewDataRegistry::Get()->Has("AnimationClip"));
    document.document->Tick(0);
    viewport.reset();
    content.reset();
    document.document.reset();
    document.moduleLease.reset();
    EXPECT_EQ(previewHost.closed, 1);
    previewHost.input.world.Reset(nullptr);
    previewHost.input.moduleLease.reset();
    AssetPreviewProviderRegistry::Get()->SetProvider(nullptr);
    extensions.reset();
    skinSummary = {};
    EXPECT_FALSE(oldPreviewAction.action.enabled());
    oldPreviewAction = {};
#endif
    world->Tick(1.f);
    EXPECT_EQ(clipRequest.Poll().state, AnimationSourceState::Error);
    EXPECT_EQ(rigRequest.Poll().state, AnimationSourceState::Error);
    AssetExecutor::Get()->WaitForAll();
    world.Reset(nullptr);
    clipRequest = {}; rigRequest = {};
    // Reflection is still callable after all runtime requests and manager leases are gone.
    const auto *retainedType = SerializationContext::Get()->FindType("AnimationComponent");
    ASSERT_NE(retainedType, nullptr);
    auto reflectedComponent = MakeAny(retainedType->info->registeredId);
    EXPECT_TRUE(reflectedComponent);
    reflectedComponent = {};
    Interface<ISystemNotify>::Get()->UnRegister();
}

#if ANIMATION_TEST_COOK
TEST(AnimationModules, MetadataOwnersSurviveCookRuntimeAndEditorReload)
{
    struct SystemNotify final : ISystemNotify {
        void SetExit() override
        {
        }
        ModuleManager *GetModuleManager() const override
        {
            return manager;
        }
        ModuleManager *manager = nullptr;
    } system;
    Interface<ISystemNotify>::Get()->Register(system);
    const TypeNode *componentType = nullptr;
    AssetBuilder   *graphBuilder  = nullptr;
    size_t          typeCount     = 0;
    for (int session = 0; session < 2; ++session) {
        ModuleManager manager;
        system.manager = &manager;
        manager.RegisterModule({"Aurora.Render", {"Aurora.Metadata"}});
        manager.RegisterModule({"Aurora.Cook", {"Aurora.Metadata"}});
        manager.RegisterModule({"Content.Module", {"Content.Metadata"}});
        manager.RegisterModule({"Content.Builder.Module", {"Content.Metadata"}});
        manager.RegisterModule({"Animation.World", {"Animation.GraphAssets", "Content.Module", "Aurora.Render"}});
        manager.RegisterModule({"Animation.Builder", {"Animation.GraphAssets"}});
    #if ANIMATION_TEST_EDITOR
        manager.RegisterModule({"Aurora.Render.Editor", {"Aurora.Metadata"}});
        manager.RegisterModule({"Content.Views", {"Content.Metadata"}});
        manager.RegisterModule({"Animation.Editor", {"Animation.GraphAssets", "Animation.World", "Aurora.Render.Editor", "Content.Views"}});
    #endif
        manager.TopoSort();
        StartArguments args;
        args.args = {"Animation.ModuleTest", "--aurora-provider-only"};
        ASSERT_TRUE(manager.LoadModules(args));
        auto *context        = SerializationContext::Get();
        auto *currentType    = context->FindType("AnimationComponent");
        auto *currentBuilder = AssetBuilderManager::Get()->QueryBuilder(".graph");
        ASSERT_NE(currentType, nullptr);
        ASSERT_NE(currentBuilder, nullptr);
        if (session == 0) {
            componentType = currentType;
            graphBuilder  = currentBuilder;
            typeCount     = context->GetAllTypes().size();
        } else {
            EXPECT_EQ(currentType, componentType);
            EXPECT_EQ(currentBuilder, graphBuilder);
            EXPECT_EQ(context->GetAllTypes().size(), typeCount);
        }
    #if ANIMATION_TEST_EDITOR
        auto *provider = sky::editor::AssetViewProviderRegistry::Get()->Find("AnimationGraph");
        ASSERT_NE(provider, nullptr);
        for (int window = 0; window < 2; ++window) {
            auto document = provider->OpenDocument(Uuid::Create(), "AnimationGraph");
            ASSERT_NE(document.document, nullptr);
            EXPECT_NE(document.document->CreateContent(nullptr), nullptr);
            EXPECT_NE(document.document->CreatePreview(nullptr), nullptr);
        }
    #endif
        manager.UnLoadModules();
        system.manager = nullptr;
    #if ANIMATION_TEST_EDITOR
        sky::editor::EditorExtensionRegistry::Get()->TakeAll();
    #endif
    }
    EXPECT_TRUE(MakeAny(componentType->info->registeredId));
    EXPECT_EQ(AssetBuilderManager::Get()->QueryBuilder(".graph"), graphBuilder);
    EXPECT_EQ(graphBuilder->QueryType(".graph"), "AnimationGraph");
    Interface<ISystemNotify>::Get()->UnRegister();
}
#endif

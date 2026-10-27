#include "WorldEditorState.h"
#include "ReflectionTestEnvironment.h"

#include <animation/IAnimationInstanceControl.h>
#include <aurora/adaptor/AuroraReflection.h>
#include <aurora/adaptor/components/EnvironmentComponent.h>
#include <aurora/adaptor/components/SkeletalMeshComponent.h>
#include <aurora/adaptor/components/StaticMeshComponent.h>
#include <aurora/adaptor/renderer/SceneAssetSource.h>
#include <editor/core/EditorCore.h>
#include <editor/core/actor/EditorActorCreation.h>
#include <editor/core/property/PropertyModel.h>
#include <editor/shell/ReflectedInspectorPanel.h>
#include <filesystem>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/GeneratedAssetCatalog.h>
#include <framework/interface/Interface.h>
#include <framework/world/ComponentFactory.h>
#include <framework/world/TransformComponent.h>
#include <framework/world/WorldSubSystemRegistry.h>
#include <gtest/gtest.h>

using namespace sky;
using namespace sky::editor;

namespace {
    struct AuthoringAnimationData {
        Uuid skeleton;
    };
    class AuthoringAnimationComponent final : public ComponentAdaptor<AuthoringAnimationData> {
    public:
        COMPONENT_RUNTIME_INFO(AuthoringAnimationComponent)
        void SetSkeleton(const Uuid &id)
        {
            data.skeleton = id;
        }
        const Uuid &GetSkeleton() const
        {
            return data.skeleton;
        }
    };
    struct AuthoringAnimationControl final : IAnimationInstanceControl {
        AuthoringAnimationControl()
        {
            WorldSubSystemRegistration registration;
            registration.factory = [](World &, const Any &) { return std::make_unique<IWorldSubSystem>(); };
            EXPECT_TRUE(WorldSubSystemRegistry::Get().Register(Name("TestAnimation"), std::move(registration)));
            Interface<IAnimationInstanceControl>::Get()->Register(*this);
        }
        ~AuthoringAnimationControl() override
        {
            WorldSubSystemRegistry::Get().Unregister(Name("TestAnimation"));
            Interface<IAnimationInstanceControl>::Get()->UnRegister();
        }
        Uuid GetComponentType() const override
        {
            return TypeInfo<AuthoringAnimationComponent>::RegisteredId();
        }
        std::string_view GetWorldSystemName() const override
        {
            return "TestAnimation";
        }
        AnimationInstanceStatus Find(const World *, const Uuid &) const override
        {
            return {};
        }
        AnimationStatus Control(const AnimationInstanceHandle &, AnimationControlOp, float) override
        {
            return {};
        }
        AnimationStatus SetParameter(const AnimationInstanceHandle &, const Uuid &, const AnimationParam &) override
        {
            return {};
        }
    };
    const auto *worldMetadata = sky::editor::test::AddReflectionEnvironment([] {
        auto *context = SerializationContext::Get();
        aurora::AuroraReflection(context);
        context->Register<AuthoringAnimationData>("AuthoringAnimationData").Member<&AuthoringAnimationData::skeleton>("skeleton");
        context->Register<AuthoringAnimationComponent>("AuthoringAnimationComponent")
            .Member<&AuthoringAnimationComponent::SetSkeleton, &AuthoringAnimationComponent::GetSkeleton>("skeleton");
        ComponentFactory::Get()->RegisterComponent<AuthoringAnimationComponent>("Test", {Name("TestAnimation")});
        RegisterBuiltinActorCreators();
    });

    class WorldSkinDefaultsTest : public testing::Test {
    protected:
        void SetUp() override
        {
            auto *context = SerializationContext::Get();
            EditorCore::Init();
            root       = std::filesystem::temp_directory_path() / ("sky-world-skin-defaults-" + Uuid::Create().ToString());
            filesystem = new NativeFileSystem(FilePath(root));
            AssetDataBase::Get()->Reset();
            AssetDataBase::Get()->SetWorkSpaceFs(filesystem);
        }
        void TearDown() override
        {
            EditorCore::Shutdown();
            AssetDataBase::Get()->Reset();
            AssetDataBase::Get()->SetWorkSpaceFs({});
            filesystem.Reset(nullptr);
            std::error_code error;
            std::filesystem::remove_all(root, error);
            EXPECT_FALSE(error);
        }
        std::filesystem::path root;
        NativeFileSystemPtr   filesystem;
    };
} // namespace

TEST_F(WorldSkinDefaultsTest, DeferredComponentCreationRestoresObjectIdentityAndBindingAcrossUndoAndReopen)
{
    auto *context = SerializationContext::Get();

    AuthoringAnimationControl control;
    WorldDocument             document((root / "components.world").string());
    SelectionService          selection;
    WorldEditorState          editor(selection);
    editor.Bind(&document);
    editor.Create("empty");
    auto      *actor       = document.GetWorld()->GetActors().front().get();
    const auto actorId     = actor->GetUuid();
    const auto skeletalId  = TypeInfo<aurora::SkeletalMeshComponent>::RegisteredId();
    const auto animationId = control.GetComponentType();
    EXPECT_FALSE(editor.RequestAddComponent(Uuid::Create()));
    ASSERT_TRUE(editor.RequestAddComponent(skeletalId));
    EXPECT_FALSE(actor->GetComponent(skeletalId));
    {
        ReflectedInspectorPanel inspector(nullptr, &editor, &selection, "Inspector");
        EXPECT_EQ(inspector.Form().GetSections().size(), 1u);
        editor.Tick();
        EXPECT_EQ(inspector.Form().GetSections().size(), 2u);
        auto *skeletal = actor->GetComponent<aurora::SkeletalMeshComponent>();
        ASSERT_TRUE(skeletal);
        const auto rig = Uuid::Create();
        skeletal->SetSkeletonUuid(rig);
        EXPECT_FALSE(editor.RequestAddComponent(skeletalId));
        ASSERT_TRUE(editor.RequestAddComponent(animationId));
        selection.Clear();
        editor.Tick();
        auto *animation = actor->GetComponent<AuthoringAnimationComponent>();
        ASSERT_TRUE(animation);
        EXPECT_EQ(animation->GetSkeleton(), rig);
        EXPECT_EQ(skeletal->GetPoseMode(), aurora::SkeletalPoseMode::SameActorAnimation);
        bool enabled = false;
        ASSERT_TRUE(document.IsSubSystemEnabled("TestAnimation", enabled));
        EXPECT_TRUE(enabled);
        selection.SetSelection({{SelectionType::ENTITY, actorId}});
        ASSERT_EQ(inspector.Form().GetSections().size(), 3u);
        PropertyModel model({animation->GetReflectedObject(), GetTypeNode(animationId)});
        const auto    changedRig = Uuid::Create();
        auto         &commands   = EditorCore::GetCommandService();
        commands.Execute(editor.MakeEditCommand(actorId, model.GetDescriptors().front(), Any(changedRig)));
        EXPECT_EQ(animation->GetSkeleton(), changedRig);
        ASSERT_TRUE(commands.Undo());
        ASSERT_TRUE(commands.Undo());
        editor.Tick();
        EXPECT_FALSE(actor->GetComponent(animationId));
        EXPECT_EQ(inspector.Form().GetSections().size(), 2u);
        EXPECT_EQ(skeletal->GetPoseMode(), aurora::SkeletalPoseMode::ReferencePose);
        EXPECT_FALSE(document.IsSubSystemEnabled("TestAnimation", enabled));
        ASSERT_TRUE(commands.Redo());
        editor.Tick();
        EXPECT_EQ(actor->GetComponent(animationId), animation);
        ASSERT_TRUE(document.IsSubSystemEnabled("TestAnimation", enabled));
        EXPECT_TRUE(enabled);
        EXPECT_EQ(inspector.Form().GetSections().size(), 3u);
        ASSERT_TRUE(commands.Redo());
        EXPECT_EQ(animation->GetSkeleton(), changedRig);
        editor.DeleteSelection();
        ASSERT_TRUE(commands.Undo());
        EXPECT_EQ(document.GetWorld()->GetActorByUuid(actorId), actor);
        ASSERT_TRUE(document.Save());
        WorldDocument reopened((root / "components.world").string());
        ASSERT_TRUE(reopened.Load());
        auto *restored = reopened.GetWorld()->GetActorByUuid(actorId);
        ASSERT_TRUE(restored);
        EXPECT_EQ(restored->GetComponent<AuthoringAnimationComponent>()->GetSkeleton(), changedRig);
        EXPECT_EQ(restored->GetComponent<aurora::SkeletalMeshComponent>()->GetPoseMode(), aurora::SkeletalPoseMode::SameActorAnimation);
        auto play = reopened.CreatePlayWorld();
        EXPECT_NE(play->GetSubSystem(Name("TestAnimation")), nullptr);
        editor.Bind(nullptr);
    }
}

TEST_F(WorldSkinDefaultsTest, MeshSelectionIsOneUndoableExplicitBindingAndAmbiguousImportsStayUnbound)
{
    ASSERT_TRUE(filesystem->WriteFileExclusive("character.model", std::array<uint8_t, 1>{1}));
    auto      *database = AssetDataBase::Get();
    const auto source   = database->RegisterAsset("character.model", false);
    ASSERT_TRUE(source);
    const std::array<GeneratedAssetDescriptor, 4> children{
        {{"skin:a", "AuroraSkin"}, {"rig:a", "AuroraSkeleton"}, {"skin:b", "AuroraSkin"}, {"rig:b", "AuroraSkeleton"}}};
    GeneratedAssetManifest             generated;
    std::map<std::string, std::string> defaults{{"binding:a:mesh", "$source"}, {"binding:a:skin", "skin:a"}, {"binding:a:skeleton", "rig:a"}};
    ASSERT_TRUE(database->UpdateGeneratedProducts(source->uuid, children, defaults, generated));
    std::map<std::string, Uuid> ids;
    ASSERT_TRUE(database->GetGeneratedDefaults(source->uuid, ids));
    WorldDocument    document((root / "character.world").string());
    SelectionService selection;
    WorldEditorState editor(selection);
    editor.Bind(&document);
    auto         *actor     = document.GetWorld()->CreateActor("Character");
    auto         *component = actor->AddComponent<aurora::SkeletalMeshComponent>();
    const auto    id        = actor->GetUuid();
    PropertyModel model(PropertyObject{component->GetReflectedObject(), GetTypeNode(component->GetTypeId())});
    const auto   &fields = model.GetDescriptors();
    const auto    mesh   = std::find_if(fields.begin(), fields.end(), [](const auto &field) { return field.GetName() == "mesh"; });
    ASSERT_NE(mesh, fields.end());
    auto &commands = EditorCore::GetCommandService();
    commands.Execute(editor.MakeEditCommand(id, *mesh, Any(source->uuid)));
    EXPECT_EQ(component->GetSkinUuid(), ids.at("binding:a:skin"));
    EXPECT_EQ(component->GetSkeletonUuid(), ids.at("binding:a:skeleton"));
    EXPECT_EQ(component->GetPoseMode(), aurora::SkeletalPoseMode::ReferencePose);
    ASSERT_TRUE(commands.Undo());
    EXPECT_FALSE(component->GetMeshUuid());
    EXPECT_FALSE(component->GetSkinUuid());
    EXPECT_FALSE(component->GetSkeletonUuid());
    ASSERT_TRUE(commands.Redo());
    selection.SetSelection({{SelectionType::ENTITY, id}});
    editor.DeleteSelection();
    EXPECT_FALSE(document.GetWorld()->GetActorByUuid(id));
    ASSERT_TRUE(commands.Undo());
    EXPECT_EQ(document.GetWorld()->GetActorByUuid(id)->GetComponent<aurora::SkeletalMeshComponent>(), component);
    ASSERT_TRUE(document.Save());
    WorldDocument reopened((root / "character.world").string());
    std::string   saved;
    ASSERT_TRUE(filesystem->OpenFile("character.world")->ReadString(saved));
    rapidjson::Document savedDocument;
    savedDocument.Parse(saved.c_str());
    ASSERT_TRUE(savedDocument.HasMember("assetRoots"));
    ASSERT_EQ(savedDocument["assetRoots"].Size(), 3u);
    std::map<std::string, std::string> roots;
    for (const auto &reference : savedDocument["assetRoots"].GetArray())
        roots[reference["type"].GetString()] = reference["uuid"].GetString();
    EXPECT_EQ(roots.at("AuroraMesh"), source->uuid.ToString());
    EXPECT_EQ(roots.at("AuroraSkin"), component->GetSkinUuid().ToString());
    EXPECT_EQ(roots.at("AuroraSkeleton"), component->GetSkeletonUuid().ToString());
    ASSERT_TRUE(reopened.Load());
    const auto *restored = reopened.GetWorld()->GetActorByUuid(id)->GetComponent<aurora::SkeletalMeshComponent>();
    ASSERT_TRUE(restored);
    EXPECT_EQ(restored->GetData(), component->GetData());
    const auto explicitBinding = component->GetData();
    commands.Execute(editor.MakeEditCommand(id, *mesh, Any(Uuid::Create())));
    EXPECT_EQ(component->GetSkinUuid(), explicitBinding.skin);
    EXPECT_EQ(component->GetSkeletonUuid(), explicitBinding.skeleton);
    ASSERT_TRUE(commands.Undo());
    EXPECT_EQ(component->GetData(), explicitBinding);
    commands.Clear();
    component->SetConfig({});
    defaults.insert({{"binding:b:mesh", "$source"}, {"binding:b:skin", "skin:b"}, {"binding:b:skeleton", "rig:b"}});
    ASSERT_TRUE(database->UpdateGeneratedProducts(source->uuid, children, defaults, generated));
    commands.Execute(editor.MakeEditCommand(id, *mesh, Any(source->uuid)));
    EXPECT_EQ(component->GetMeshUuid(), source->uuid);
    EXPECT_FALSE(component->GetSkinUuid());
    EXPECT_FALSE(component->GetSkeletonUuid());
    ASSERT_TRUE(commands.Undo());
    EXPECT_FALSE(component->GetMeshUuid());
    editor.Bind(nullptr);
}
TEST(WorldEditorState, CreateEditUndoDeleteAndReopenKeepLogicalSceneData)
{
    auto *context = SerializationContext::Get();
    EditorCore::Init();
    const auto       path = std::filesystem::temp_directory_path() / "sky_world_editor_scene.world";
    WorldDocument    document(path.string());
    SelectionService selection;
    WorldEditorState editor(selection);
    editor.Bind(&document);
    editor.Create("cube");
    ASSERT_EQ(document.GetWorld()->GetActors().size(), 1u);
    const auto id = document.GetWorld()->GetActors().front()->GetUuid();
    ASSERT_FALSE(editor.Resolve(id).empty());
    const auto    objects = editor.Resolve(id);
    PropertyModel model(objects.front());
    const auto   &descriptors = model.GetDescriptors();
    const auto translation = std::find_if(descriptors.begin(), descriptors.end(), [](const auto &value) { return value.GetName() == "translation"; });
    ASSERT_NE(translation, descriptors.end());
    EditorCore::GetCommandService().Execute(editor.MakeEditCommand(id, *translation, Any(Vector3{1, 2, 3})));
    ASSERT_TRUE(document.Save());
    EXPECT_FALSE(document.IsDirty());
    EXPECT_EQ(document.GetWorld()->GetActorByUuid(id)->GetComponent<TransformComponent>()->GetWorldMatrix().v[12], 1);
    EXPECT_TRUE(EditorCore::GetCommandService().Undo());
    EXPECT_TRUE(document.IsDirty());
    EXPECT_EQ(document.GetWorld()->GetActorByUuid(id)->GetComponent<TransformComponent>()->GetWorldMatrix().v[12], 0);
    EXPECT_TRUE(EditorCore::GetCommandService().Redo());
    editor.DeleteSelection();
    EXPECT_TRUE(document.GetWorld()->GetActors().empty());
    EXPECT_TRUE(EditorCore::GetCommandService().Undo());
    ASSERT_TRUE(document.GetWorld()->GetActorByUuid(id));
    EXPECT_EQ(editor.Outline().size(), 1u);
    ASSERT_TRUE(document.Save());
    WorldDocument reopened(path.string());
    ASSERT_TRUE(reopened.Load());
    auto *restored = reopened.GetWorld()->GetActorByUuid(id);
    ASSERT_TRUE(restored);
    ASSERT_TRUE(restored->GetComponent<aurora::StaticMeshComponent>());
    EXPECT_EQ(restored->GetComponent<aurora::StaticMeshComponent>()->GetMeshUuid(), aurora::ScenePrimitiveId(aurora::ScenePrimitive::CUBE));
    EXPECT_EQ(restored->GetComponent<aurora::StaticMeshComponent>()->GetMaterialUuid(), aurora::SceneLitMaterialId());
    EXPECT_EQ(restored->GetComponent<TransformComponent>()->GetWorldMatrix().v[12], 1);
    editor.Bind(&reopened);
    EXPECT_FALSE(EditorCore::GetCommandService().CanUndo());
    EXPECT_TRUE(selection.IsEmpty());
    editor.Bind(nullptr);
    EditorCore::Shutdown();
    std::error_code error;
    std::filesystem::remove(path, error);
}

TEST(WorldEditorState, ActorHierarchyAndGroupedInspectorKeepSelectionAndUndo)
{
    auto *context = SerializationContext::Get();
    EditorCore::Init();
    WorldDocument    document("");
    SelectionService selection;
    WorldEditorState editor(selection);
    editor.Bind(&document);
    editor.Create("cube");
    auto      *child   = document.GetWorld()->GetActors().back().get();
    const auto childId = child->GetUuid();
    editor.Create("plane");
    auto      *parent   = document.GetWorld()->GetActors().back().get();
    const auto parentId = parent->GetUuid();
    child->SetParent(parent);
    auto      *other = document.GetWorld()->CreateActor("No transform", false);
    const auto rows  = editor.Outline();
    ASSERT_EQ(rows.size(), 3u);
    EXPECT_EQ(rows[0].id, parentId);
    EXPECT_EQ(rows[0].depth, 0u);
    EXPECT_EQ(rows[1].id, childId);
    EXPECT_EQ(rows[1].depth, 1u);
    EXPECT_EQ(rows[2].id, other->GetUuid());
    EXPECT_EQ(rows[2].depth, 0u);
    ASSERT_EQ(editor.Resolve(parentId).size(), 2u);
    EXPECT_EQ(editor.Resolve(parentId).front().type, GetTypeNode(TypeInfo<TransformComponent>::RegisteredId()));
    EXPECT_TRUE(editor.Resolve(aurora::ScenePrimitiveId(aurora::ScenePrimitive::PLANE)).empty());
    {
        ReflectedInspectorPanel inspector(nullptr, &editor, &selection, "Inspector");
        auto                   &sections = inspector.Form().GetSections();
        ASSERT_EQ(sections.size(), 2u);
        EXPECT_EQ(sections[0].title, "TransformComponent");
        EXPECT_EQ(sections[1].title, "StaticMeshComponent");
        auto      &fields = sections[1].fields;
        const auto cast   = std::find_if(fields.begin(), fields.end(), [](const auto &field) { return field.descriptor.GetName() == "castShadow"; });
        ASSERT_NE(cast, fields.end());
        ASSERT_TRUE(inspector.Form().Edit(*cast, Any(false), EditorCore::GetCommandService()));
        EXPECT_FALSE(parent->GetComponent<aurora::StaticMeshComponent>()->GetCastShadow());
        EXPECT_TRUE(child->GetComponent<aurora::StaticMeshComponent>()->GetCastShadow());
        ASSERT_TRUE(EditorCore::GetCommandService().Undo());
        EXPECT_TRUE(parent->GetComponent<aurora::StaticMeshComponent>()->GetCastShadow());
        selection.SetSelection({{SelectionType::ASSET, parentId}});
        editor.DeleteSelection();
        EXPECT_EQ(editor.Outline().size(), 3u);
        selection.SetSelection({{SelectionType::ENTITY, childId}});
        editor.DeleteSelection();
        EXPECT_FALSE(inspector.Form().IsValid());
        EXPECT_EQ(editor.Outline().size(), 2u);
        ASSERT_TRUE(EditorCore::GetCommandService().Undo());
        EXPECT_EQ(editor.Outline().size(), 3u);
        selection.SetSelection({{SelectionType::ENTITY, childId}});
        EXPECT_EQ(inspector.Form().GetSections().size(), 2u);
        editor.Bind(nullptr);
        EXPECT_TRUE(inspector.Form().GetSections().empty());
    }
    EditorCore::Shutdown();
}

TEST(WorldEditorState, EnvironmentReflectedEditUndoAndReopenPreserveInputs)
{
    auto *context = SerializationContext::Get();
    EditorCore::Init();
    const auto       path = std::filesystem::temp_directory_path() / "sky_world_editor_environment.world";
    WorldDocument    document(path.string());
    SelectionService selection;
    WorldEditorState editor(selection);
    editor.Bind(&document);
    editor.Create("environment");
    ASSERT_EQ(document.GetWorld()->GetActors().size(), 1u);
    auto      *actor     = document.GetWorld()->GetActors().front().get();
    const auto id        = actor->GetUuid();
    auto      *component = actor->GetComponent<aurora::EnvironmentComponent>();
    ASSERT_TRUE(component);
    PropertyModel model(PropertyObject{component->GetReflectedObject(), GetTypeNode(component->GetTypeId())});
    const auto   &descriptors = model.GetDescriptors();
    const auto    edit        = [&](const char *name, Any value) {
        const auto found = std::find_if(descriptors.begin(), descriptors.end(), [&](const auto &field) { return field.GetName() == name; });
        ASSERT_NE(found, descriptors.end());
        EditorCore::GetCommandService().Execute(editor.MakeEditCommand(id, *found, std::move(value)));
    };
    edit("exposure", Any(2.f));
    EXPECT_EQ(component->GetExposure(), 2.f);
    ASSERT_TRUE(EditorCore::GetCommandService().Undo());
    EXPECT_EQ(component->GetExposure(), 0.f);
    ASSERT_TRUE(EditorCore::GetCommandService().Redo());
    const auto diffuse = Uuid::CreateWithSeed(942);
    edit("diffuseIrradiance", Any(diffuse));
    edit("intensity", Any(3.f));
    ASSERT_TRUE(document.Save());
    WorldDocument reopened(path.string());
    ASSERT_TRUE(reopened.Load());
    const auto *restored = reopened.GetWorld()->GetActorByUuid(id)->GetComponent<aurora::EnvironmentComponent>();
    ASSERT_TRUE(restored);
    EXPECT_EQ(restored->GetDiffuseIrradiance(), diffuse);
    EXPECT_EQ(restored->GetExposure(), 2.f);
    EXPECT_EQ(restored->GetIntensity(), 3.f);
    EXPECT_TRUE(restored->GetHdr());
    editor.Bind(nullptr);
    EditorCore::Shutdown();
    std::error_code error;
    std::filesystem::remove(path, error);
}

TEST_F(WorldSkinDefaultsTest, GenericComponentDependenciesAndRegisteredCreatorsUseOneUndoStep)
{
    WorldDocument    document((root / "generic.world").string());
    SelectionService selection;
    WorldEditorState editor(selection);
    editor.Bind(&document);
    ASSERT_TRUE(editor.Create("empty"));
    auto      *actor = document.GetWorld()->GetActors().front().get();
    const auto type  = TypeInfo<AuthoringAnimationComponent>::RegisteredId();
    EXPECT_FALSE(editor.CanAddComponent(type));
    WorldSubSystemRegistration registration;
    registration.factory = [](World &, const Any &) { return std::make_unique<IWorldSubSystem>(); };
    ASSERT_TRUE(WorldSubSystemRegistry::Get().Register(Name("TestAnimation"), std::move(registration)));
    struct Cleanup {
        ~Cleanup()
        {
            WorldSubSystemRegistry::Get().Unregister(Name("TestAnimation"));
            EditorActorCreation::Get()->UnRegisterCreation("test.generic");
        }
    } cleanup;
    ASSERT_TRUE(editor.RequestAddComponent(type));
    editor.Tick();
    ASSERT_NE(actor->GetComponent(type), nullptr);
    bool enabled = false;
    ASSERT_TRUE(document.IsSubSystemEnabled("TestAnimation", enabled));
    EXPECT_TRUE(enabled);
    auto &commands = EditorCore::GetCommandService();
    ASSERT_TRUE(commands.Undo());
    EXPECT_EQ(actor->GetComponent(type), nullptr);
    EXPECT_FALSE(document.IsSubSystemEnabled("TestAnimation", enabled));
    ASSERT_TRUE(commands.Redo());
    EXPECT_NE(actor->GetComponent(type), nullptr);
    ASSERT_TRUE(commands.Undo());
    const auto count = commands.GetUndoCount();
    ASSERT_TRUE(EditorActorCreation::Get()->RegisterCreation({{}, "test.generic", "Generic Actor", "Test", 0, [type](Actor &candidate) {
                                                                  candidate.SetName("Registered actor");
                                                                  return candidate.AddComponent(type) != nullptr;
                                                              }}));
    ASSERT_TRUE(editor.Create("test.generic"));
    EXPECT_EQ(commands.GetUndoCount(), count + 1);
    ASSERT_EQ(document.GetWorld()->GetActors().size(), 2u);
    ASSERT_TRUE(document.IsSubSystemEnabled("TestAnimation", enabled));
    EXPECT_TRUE(enabled);
    ASSERT_TRUE(EditorActorCreation::Get()->UnRegisterCreation("test.generic"));
    EXPECT_FALSE(editor.Create("test.generic"));
    ASSERT_TRUE(commands.Undo());
    EXPECT_EQ(document.GetWorld()->GetActors().size(), 1u);
    EXPECT_FALSE(document.IsSubSystemEnabled("TestAnimation", enabled));
    ASSERT_TRUE(commands.Redo());
    EXPECT_EQ(document.GetWorld()->GetActors().size(), 2u);
    EXPECT_TRUE(document.CreatePlayWorld());
    editor.Bind(nullptr);
}

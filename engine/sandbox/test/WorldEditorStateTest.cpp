#include "ReflectionTestEnvironment.h"
#include "WorldEditorState.h"

#include <aurora/adaptor/AuroraReflection.h>
#include <aurora/adaptor/components/EnvironmentComponent.h>
#include <aurora/adaptor/components/StaticMeshComponent.h>
#include <aurora/adaptor/renderer/SceneAssetSource.h>
#include <editor/core/EditorCore.h>
#include <editor/core/property/PropertyModel.h>
#include <editor/shell/ReflectedInspectorPanel.h>
#include <filesystem>
#include <framework/world/TransformComponent.h>
#include <gtest/gtest.h>

using namespace sky;
using namespace sky::editor;
namespace {
    const auto *auroraMetadata = sky::editor::test::AddReflectionEnvironment([] {
        aurora::AuroraReflection(SerializationContext::Get());
    });
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
    editor.Create(EditorSceneObject::CUBE);
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
    editor.Create(EditorSceneObject::CUBE);
    auto      *child   = document.GetWorld()->GetActors().back().get();
    const auto childId = child->GetUuid();
    editor.Create(EditorSceneObject::PLANE);
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
    editor.Create(EditorSceneObject::ENVIRONMENT);
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

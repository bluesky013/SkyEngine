//
// Created on 2026/10/22.
//

#include <algorithm>
#include <cmath>
#include <core/logger/Logger.h>
#include <editor/core/viewport/ViewportController.h>
#include <filesystem>
#include <framework/serialization/SerializationContext.h>
#include <framework/world/TransformComponent.h>
#include <gtest/gtest.h>
#include <limits>

using namespace sky;
using namespace sky::editor;

TEST(ViewportControls, CameraProjectionRayAndNormalizedFlight)
{
    EditorCamera camera;
    Vector2      point;
    ASSERT_TRUE(camera.Project({1, 1, 0}, {800, 600}, point));
    EXPECT_GT(point.x, 400);
    EXPECT_LT(point.y, 300);
    auto direction = Vector3{1, 1, 0} - camera.position;
    direction.Normalize();
    EXPECT_LT((camera.Ray(point, {800, 600}) - direction).Length(), 1.e-5f);
    EXPECT_FALSE(camera.Project({0, 0, 6}, {800, 600}, point));
    EditorCamera split = camera;
    camera.Fly({1, 0, 1}, 1.f, 5.f);
    for (int i = 0; i < 100; ++i)
        split.Fly({1, 0, 1}, .01f, 5.f);
    EXPECT_NEAR((camera.position - Vector3{0, 0, 5}).Length(), 5.f, 1.e-4f);
    EXPECT_LT((split.position - camera.position).Length(), 1.e-4f);
}

TEST(ViewportControls, ProviderGeometryControlsPickingAndRejectsInvalidOutput)
{
    DefaultGizmoVisualProvider provider;
    GizmoDrawContext           context;
    context.extent = {800, 600};
    GizmoGeometry geometry;
    std::string   error;
    ASSERT_TRUE(provider.Build(context, geometry, error));
    EXPECT_EQ(HitTestGizmo(geometry, {470, 300}, 4), GizmoHandle::X);
    geometry = {{GizmoHandle::Y, {{10, 10}, {50, 10}}, 0, 2}};
    EXPECT_EQ(HitTestGizmo(geometry, {30, 10}, 4), GizmoHandle::Y);
    EXPECT_EQ(HitTestGizmo(geometry, {470, 300}, 4), GizmoHandle::NONE);
    geometry.front().points.front().x = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(ValidateGizmoGeometry(geometry, error));
    EXPECT_FALSE(error.empty());
    geometry = {{GizmoHandle::XY, {{0, 0}, {50, 0}, {20, 20}, {50, 50}, {0, 50}}, 0, 2, true, true}};
    EXPECT_FALSE(ValidateGizmoGeometry(geometry, error));
    geometry = {{GizmoHandle::XY, {{0, 0}, {50, 0}, {50, 50}, {0, 50}}, 0, 2, true, true}};
    EXPECT_TRUE(ValidateGizmoGeometry(geometry, error));
    EXPECT_EQ(HitTestGizmo(geometry, {25, 25}, 0), GizmoHandle::XY);
    geometry.front().closed = false;
    EXPECT_EQ(HitTestGizmo(geometry, {-2, 25}, 3), GizmoHandle::XY);
    geometry.front().filled = false;
    EXPECT_EQ(HitTestGizmo(geometry, {-2, 25}, 3), GizmoHandle::NONE);
}

TEST(ViewportControls, PitchedFlightKeepsSpeedForVerticalAndForwardCombinations)
{
    for (const float pitch : {-.7853982f, .7853982f, 1.553343f}) {
        for (const auto input : {Vector3{0, 1, 1}, Vector3{0, -1, 1}, Vector3{1, 1, 1}, Vector3{0, .5f, 0}}) {
            EditorCamera camera;
            camera.pitch      = pitch;
            camera.yaw        = .3f;
            const auto before = camera.position;
            const auto pivot  = camera.pivot;
            camera.Fly(input, 1.f, 5.f);
            EXPECT_NEAR((camera.position - before).Length(), std::min(input.Length(), 1.f) * 5.f, 1.e-5f);
            EXPECT_LT((camera.position - before - (camera.pivot - pivot)).Length(), 1.e-5f);
        }
    }
}

TEST(ViewportControls, MinimumRotationSnapCommitsAndUndoRedoPreserveIt)
{
    WorldDocument          document("");
    auto                  *actor     = document.GetWorld()->CreateActor("Small rotation");
    auto                  *transform = actor->GetComponent<TransformComponent>();
    SelectionService       selection;
    CommandService         commands;
    PropertyChangeNotifier changes;
    ViewportController     controller(selection, commands, changes);
    controller.Bind(&document);
    selection.SetSelection({{SelectionType::ENTITY, actor->GetUuid()}});
    controller.SetTool(GizmoTool::ROTATE);
    controller.ToggleSnap();
    controller.settings.rotationStep = .1f;
    ASSERT_TRUE(controller.BeginDrag(GizmoHandle::Z, {480, 300}, {800, 600}, {}));
    const float angle = .1f * .01745329252f;
    controller.MovePointer({400 + 80 * std::cos(angle), 300 - 80 * std::sin(angle)});
    const auto expected = transform->GetLocalTransform().rotation;
    EXPECT_NEAR((expected * Vector3{1, 0, 0}).y, std::sin(angle), 1.e-6f);
    controller.Release();
    ASSERT_EQ(commands.GetUndoCount(), 1u);
    EXPECT_TRUE(document.IsDirty());
    EXPECT_FLOAT_EQ(transform->GetLocalTransform().rotation.z, expected.z);
    ASSERT_TRUE(commands.Undo());
    EXPECT_FLOAT_EQ(transform->GetLocalTransform().rotation.z, 0.f);
    ASSERT_TRUE(commands.Redo());
    EXPECT_FLOAT_EQ(transform->GetLocalTransform().rotation.z, expected.z);
    ASSERT_TRUE(controller.BeginDrag(GizmoHandle::Z, {480, 300}, {800, 600}, {}));
    controller.MovePointer({480, 300});
    controller.Release();
    EXPECT_EQ(commands.GetUndoCount(), 1u);
    controller.Bind(nullptr);
}

TEST(ViewportControls, ParentedTranslationSnapAndSingularParent)
{
    Transform parent, local;
    parent.rotation   = Quaternion(.7f, Vector3{0, 0, 1});
    parent.scale      = {2, 3, 4};
    local.translation = {.13f, .17f, 0};
    GizmoDrawContext context;
    context.extent = {800, 600};
    context.world  = parent * local;
    Vector2 start;
    ASSERT_TRUE(context.camera.Project(context.world.translation, context.extent, start));
    GizmoDrag   drag;
    std::string error;
    ASSERT_TRUE(drag.Begin(context, GizmoHandle::X, start, local, parent, error));
    Transform               output;
    ViewportControlSettings settings;
    ASSERT_TRUE(drag.Update(start + Vector2{90, 0}, true, settings, output));
    const auto movement = (parent * output).translation - context.world.translation;
    EXPECT_NEAR(movement.y, 0.f, 1.e-5f);
    EXPECT_NEAR(movement.x / settings.translationStep, std::round(movement.x / settings.translationStep), 1.e-4f);
    parent.scale.x = 0;
    EXPECT_FALSE(drag.Begin(context, GizmoHandle::X, start, local, parent, error));
}

TEST(ViewportControls, PreviewCancelCommitUndoAndLostTarget)
{
    WorldDocument          document("");
    auto                  *actor     = document.GetWorld()->CreateActor("Target");
    auto                  *transform = actor->GetComponent<TransformComponent>();
    SelectionService       selection;
    CommandService         commands;
    PropertyChangeNotifier changes;
    ViewportController     controller(selection, commands, changes);
    controller.Bind(&document);
    selection.SetSelection({{SelectionType::ENTITY, actor->GetUuid()}});
    ASSERT_TRUE(controller.BeginDrag(GizmoHandle::X, {430, 300}, {800, 600}, {}));
    controller.MovePointer({500, 300});
    EXPECT_GT(transform->GetLocalTranslation().x, 0.f);
    EXPECT_EQ(commands.GetUndoCount(), 0u);
    EXPECT_FALSE(document.IsDirty());
    controller.Cancel();
    EXPECT_EQ(transform->GetLocalTranslation().x, 0.f);
    ASSERT_TRUE(controller.BeginDrag(GizmoHandle::X, {430, 300}, {800, 600}, {}));
    controller.MovePointer({500, 300});
    const float finalX = transform->GetLocalTranslation().x;
    controller.Release();
    EXPECT_EQ(commands.GetUndoCount(), 1u);
    EXPECT_TRUE(document.IsDirty());
    ASSERT_TRUE(commands.Undo());
    EXPECT_EQ(transform->GetLocalTranslation().x, 0.f);
    ASSERT_TRUE(commands.Redo());
    EXPECT_EQ(transform->GetLocalTranslation().x, finalX);
    ASSERT_TRUE(controller.BeginDrag(GizmoHandle::X, {430, 300}, {800, 600}, {}));
    controller.MovePointer({450, 300});
    auto removed = document.GetWorld()->DetachFromWorld(actor);
    removed.reset();
    controller.Tick(.01f);
    EXPECT_FALSE(controller.HasGesture());
    bool reportedMissingTarget = false;
    Logger::SetOutputCallback([&](const char *, const char *level, const char *message) {
        reportedMissingTarget = std::string(level) == "ERROR" && std::string(message).find("target") != std::string::npos;
    });
    commands.Undo();
    Logger::SetOutputCallback({});
    EXPECT_TRUE(reportedMissingTarget);
    controller.Bind(nullptr);
    commands.Clear();
}

TEST(ViewportControls, CommittedTransformSurvivesSaveAndReopen)
{
    const auto             path = std::filesystem::temp_directory_path() / "sky_gizmo_transform_roundtrip.world";
    WorldDocument          document(path.string());
    auto                  *actor = document.GetWorld()->CreateActor("Saved gizmo target");
    const auto             id    = actor->GetUuid();
    SelectionService       selection;
    CommandService         commands;
    PropertyChangeNotifier changes;
    ViewportController     controller(selection, commands, changes);
    controller.Bind(&document);
    selection.SetSelection({{SelectionType::ENTITY, id}});
    ASSERT_TRUE(controller.BeginDrag(GizmoHandle::X, {430, 300}, {800, 600}, {}));
    controller.MovePointer({510, 300});
    controller.Release();
    const auto expected = actor->GetComponent<TransformComponent>()->GetLocalTranslation();
    ASSERT_TRUE(document.Save());
    EXPECT_FALSE(document.IsDirty());
    WorldDocument reopened(path.string());
    ASSERT_TRUE(reopened.Load());
    auto *saved = reopened.GetWorld()->GetActorByUuid(id);
    ASSERT_NE(saved, nullptr);
    EXPECT_LT((saved->GetComponent<TransformComponent>()->GetLocalTranslation() - expected).Length(), 1.e-6f);
    controller.Bind(nullptr);
    std::error_code error;
    std::filesystem::remove(path, error);
    EXPECT_FALSE(error);
}

TEST(ViewportControls, CameraSpeedBoostAndCancellation)
{
    WorldDocument          document("");
    SelectionService       selection;
    CommandService         commands;
    PropertyChangeNotifier changes;
    ViewportController     controller(selection, commands, changes);
    controller.Bind(&document);
    controller.settings.speed = 2.f;
    controller.BeginNavigation(ViewportNavigation::LOOK, {}, 600);
    EXPECT_TRUE(controller.NavigationKey('W', true));
    controller.Tick(1);
    EXPECT_NEAR(controller.camera.position.z, 3.f, 1.e-5f);
    controller.NavigationKey(0x10, true);
    controller.Tick(1);
    EXPECT_NEAR(controller.camera.position.z, -5.f, 1.e-5f);
    controller.Cancel();
    controller.Tick(1);
    EXPECT_NEAR(controller.camera.position.z, -5.f, 1.e-5f);
    EXPECT_FALSE(document.IsDirty());
    EXPECT_EQ(commands.GetUndoCount(), 0u);
}

TEST(ViewportControls, LookPanOrbitFocusAndPlayRemainEditorOnly)
{
    WorldDocument document("");
    auto         *actor = document.GetWorld()->CreateActor("Focus target");
    actor->GetComponent<TransformComponent>()->SetLocalTranslation({2, 1, 0});
    SelectionService       selection;
    CommandService         commands;
    PropertyChangeNotifier changes;
    ViewportController     controller(selection, commands, changes);
    controller.Bind(&document);
    selection.SetSelection({{SelectionType::ENTITY, actor->GetUuid()}});
    controller.BeginNavigation(ViewportNavigation::LOOK, {400, 300}, 600);
    controller.MovePointer({440, 280});
    EXPECT_NEAR(controller.camera.yaw, 10.f * .01745329252f, 1.e-5f);
    EXPECT_GT(controller.camera.pitch, 0);
    controller.Release();
    const auto before = controller.camera.position;
    controller.BeginNavigation(ViewportNavigation::PAN, {400, 300}, 600);
    controller.MovePointer({430, 300});
    EXPECT_LT((controller.camera.position - before).Dot(controller.camera.Right()), 0);
    EXPECT_TRUE(controller.FocusSelection());
    Vector2 projected;
    ASSERT_TRUE(controller.camera.Project({2, 1, 0}, {800, 600}, projected));
    EXPECT_NEAR(projected.x, 400, .001f);
    EXPECT_NEAR(projected.y, 300, .001f);
    controller.BeginNavigation(ViewportNavigation::ORBIT, {400, 300}, 600);
    const float distance = (controller.camera.position - controller.camera.pivot).Length();
    controller.MovePointer({460, 320});
    EXPECT_NEAR((controller.camera.position - controller.camera.pivot).Length(), distance, .0001f);
    const auto editPosition = controller.camera.position;
    controller.SetEditing(false);
    EXPECT_FALSE(controller.HasGesture());
    controller.BeginNavigation(ViewportNavigation::LOOK, {}, 600);
    EXPECT_FALSE(controller.HasGesture());
    controller.SetEditing(true);
    EXPECT_LT((controller.camera.position - editPosition).Length(), .0001f);
    EXPECT_FALSE(document.IsDirty());
    EXPECT_EQ(commands.GetUndoCount(), 0u);
}

TEST(ViewportControls, RotationSnapAndScalePreserveSigns)
{
    GizmoDrawContext context;
    context.extent = {800, 600};
    context.tool   = GizmoTool::ROTATE;
    GizmoDrag   drag;
    std::string error;
    ASSERT_TRUE(drag.Begin(context, GizmoHandle::Z, {480, 300}, {}, {}, error));
    Transform output;
    ASSERT_TRUE(drag.Update({400, 220}, true, {}, output));
    EXPECT_LT((output.rotation * Vector3{1, 0, 0} - Vector3{0, 1, 0}).Length(), 1.e-5f);
    context.tool        = GizmoTool::SCALE;
    context.world.scale = {-1, 2, 3};
    ASSERT_TRUE(drag.Begin(context, GizmoHandle::UNIFORM, {400, 300}, context.world, {}, error));
    ASSERT_TRUE(drag.Update({440, 260}, false, {}, output));
    EXPECT_FLOAT_EQ(output.scale.x, -2);
    EXPECT_FLOAT_EQ(output.scale.y, 4);
    ASSERT_TRUE(drag.Update({0, 700}, false, {}, output));
    EXPECT_LT(output.scale.x, 0);
    EXPECT_GT(output.scale.y, 0);
}

TEST(ViewportControls, HistoryCancelsPreviewBeforeUndoAndDocumentSwitchClearsCommands)
{
    WorldDocument          document("");
    auto                  *actor = document.GetWorld()->CreateActor("Target");
    SelectionService       selection;
    CommandService         commands;
    PropertyChangeNotifier changes;
    ViewportController     controller(selection, commands, changes);
    controller.Bind(&document);
    selection.SetSelection({{SelectionType::ENTITY, actor->GetUuid()}});
    ASSERT_TRUE(controller.BeginDrag(GizmoHandle::X, {430, 300}, {800, 600}, {}));
    controller.MovePointer({500, 300});
    controller.Release();
    ASSERT_TRUE(controller.BeginDrag(GizmoHandle::X, {430, 300}, {800, 600}, {}));
    controller.MovePointer({550, 300});
    ASSERT_TRUE(commands.Undo());
    EXPECT_FALSE(controller.HasGesture());
    EXPECT_EQ(actor->GetComponent<TransformComponent>()->GetLocalTranslation().x, 0);
    controller.Bind(nullptr);
    EXPECT_FALSE(commands.CanRedo());
}

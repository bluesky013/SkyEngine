//
// Created on 2026/10/22.
//

#include <cmath>
#include <editor/core/extension/EditorActionRegistry.h>
#include <editor/core/extension/EditorScopeStack.h>
#include <editor/shell/EditorShell.h>
#include <editor/shell/SceneViewport.h>
#include <editor/ui/UiTheme.h>
#include <editor/widgets/ActionBarBuilder.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/world/TransformComponent.h>
#include <gtest/gtest.h>
#include <ui/UIPaintContext.h>

using namespace sky;
using namespace sky::editor;
namespace {
    class OffsetVisual final : public IGizmoVisualProvider {
    public:
        bool Build(const GizmoDrawContext &, GizmoGeometry &geometry, std::string &error) const override
        {
            geometry = {{GizmoHandle::X, {{20, 20}, {100, 20}}, 0xFFFFFFFF, 2}};
            error.clear();
            return true;
        }
    };
    ui::UIPointerEvent Pointer(ui::UIPointerAction action, float x, float y, uint32_t button = 0)
    {
        ui::UIPointerEvent event;
        event.action = action;
        event.x      = x;
        event.y      = y;
        event.button = button;
        return event;
    }
    class SceneViewportTest : public testing::Test {
    protected:
        void SetUp() override
        {
            if (!TypeInfo<TransformComponent>::RegisteredId())
                TransformComponent::Reflect(SerializationContext::Get());
            EditorActionRegistry::Get()->Clear();
            EditorScopeStack::Get()->Clear();
            SetDefaultUiTheme(MakeDarkTheme());
            actor      = document.GetWorld()->CreateActor("Target");
            controller = std::make_shared<ViewportController>(selection, commands, changes);
            controller->Bind(&document);
            selection.SetSelection({{SelectionType::ENTITY, actor->GetUuid()}});
        }
        void TearDown() override
        {
            controller->Bind(nullptr);
            EditorActionRegistry::Get()->Clear();
            EditorScopeStack::Get()->Clear();
        }
        WorldDocument                       document{""};
        SelectionService                    selection;
        CommandService                      commands;
        PropertyChangeNotifier              changes;
        Actor                              *actor = nullptr;
        std::shared_ptr<ViewportController> controller;
    };
} // namespace

namespace {
    class PreparedPicker final : public IViewportPicker {
    public:
        ViewportPickContext     context;
        ViewportPickStatus      status = ViewportPickStatus::HIT;
        Uuid                    actor;
        uint64_t                serial = 0, queries = 0;
        ViewportPickRequest     last;
        ViewportSelectionBounds bounds;
        ViewportSelectionBounds SelectionBounds(const Uuid &) const override
        {
            return bounds;
        }
        ViewportPickResult Submit(const ViewportPickRequest &request) override
        {
            last = request;
            return {.context = request.context};
        }
        ViewportPickResult Poll(const ViewportPickContext &) override
        {
            return Query(last);
        }
        void Cancel(const ViewportPickContext &) override
        {
        }
        ViewportPickContext Context() const override
        {
            return context;
        }
        uint64_t PreparationSerial() const override
        {
            return serial;
        }
        ViewportPickResult Query(const ViewportPickRequest &request) override
        {
            ++queries;
            last = request;
            return {.context = request.context, .status = status, .actor = actor, .message = "Picker test state"};
        }
        void Advance(ViewportController &controller, uint64_t scene = 1)
        {
            context       = controller.PickContext();
            context.scene = scene;
            ++serial;
            controller.ProcessPicking();
        }
    };
} // namespace

TEST_F(SceneViewportTest, ClickCandidatesSelectWithoutCommandsOnMainAndFloatingSurfaces)
{
    auto picker   = std::make_shared<PreparedPicker>();
    picker->actor = actor->GetUuid();
    controller->SetPicker(picker);
    controller->SetTool(GizmoTool::SELECT);
    for (const uint32_t surface : {0u, 7u}) {
        LayoutModel layout;
        layout.SetDefault({"viewport"});
        PanelRegistry registry;
        registry.Register({"viewport", "Viewport"});
        EditorShell shell;
        shell.SetLayout(&layout);
        shell.SetPanelRegistry(&registry);
        shell.RegisterPanelView("viewport", [&]() { return std::make_unique<SceneViewport>(controller); });
        shell.Rebuild();
        shell.Layout(1000, 800);
        if (surface) {
            FloatingPanel floating;
            floating.panelId = "viewport";
            shell.FloatPanelToSurface("viewport", surface, floating);
            ui::UIPaintContext paint;
            paint.Begin({0, 0, 600, 400});
            shell.PaintSurface(surface, paint, 600, 400);
        }
        controller->SetPickSurface(surface);
        const auto  bounds = shell.GetPanelView("viewport")->GetBounds();
        const float x = (bounds.left + bounds.right) * .5f, y = (bounds.top + bounds.bottom) * .5f;
        const auto  route = [&](ui::UIPointerAction action, float px, float py) { shell.DispatchPointerToSurface(surface, Pointer(action, px, py)); };
        selection.Clear();
        picker->status = ViewportPickStatus::HIT;
        route(ui::UIPointerAction::DOWN, x, y);
        EXPECT_TRUE(shell.HasPointerCapture(surface));
        route(ui::UIPointerAction::UP, x, y);
        EXPECT_FALSE(shell.HasPointerCapture(surface));
        EXPECT_TRUE(selection.IsEmpty());
        picker->Advance(*controller);
        ASSERT_EQ(selection.GetSelection().size(), 1u);
        EXPECT_EQ(selection.GetSelection()[0].id, actor->GetUuid());
        EXPECT_NEAR(picker->last.direction.x, 0, 1.e-5f);
        EXPECT_NEAR(picker->last.direction.y, 0, 1.e-5f);
        EXPECT_EQ(picker->last.context.surface, surface);
        const auto queries = picker->queries;
        route(ui::UIPointerAction::DOWN, x, y);
        route(ui::UIPointerAction::MOVE, x + 20, y);
        route(ui::UIPointerAction::UP, x, y);
        picker->Advance(*controller);
        EXPECT_EQ(picker->queries, queries);
        route(ui::UIPointerAction::DOWN, x, y);
        shell.DispatchPointerToSurface(surface, Pointer(ui::UIPointerAction::DOWN, x, y, 1));
        route(ui::UIPointerAction::UP, x, y);
        picker->Advance(*controller);
        EXPECT_EQ(picker->queries, queries);
        picker->status = ViewportPickStatus::MISS;
        route(ui::UIPointerAction::DOWN, x, y);
        route(ui::UIPointerAction::UP, x, y);
        picker->Advance(*controller);
        EXPECT_TRUE(selection.IsEmpty());
        EXPECT_EQ(commands.GetUndoCount(), 0u);
        EXPECT_FALSE(document.IsDirty());
    }
}

TEST_F(SceneViewportTest, PendingAndUnsupportedPreserveSelectionAndStaleContextsCancel)
{
    auto  picker  = std::make_shared<PreparedPicker>();
    auto *other   = document.GetWorld()->CreateActor("Other");
    picker->actor = other->GetUuid();
    controller->SetPicker(picker);
    controller->SetTool(GizmoTool::SELECT);
    SceneViewport viewport(controller);
    viewport.Arrange({10, 20, 810, 620});
    const auto click = [&]() {
        viewport.OnPointerEvent(Pointer(ui::UIPointerAction::DOWN, 410, 320));
        viewport.OnPointerEvent(Pointer(ui::UIPointerAction::UP, 410, 320));
    };
    picker->status = ViewportPickStatus::PENDING;
    click();
    picker->Advance(*controller);
    EXPECT_EQ(selection.GetSelection()[0].id, actor->GetUuid());
    EXPECT_FALSE(controller->Error().empty());
    picker->status = ViewportPickStatus::HIT;
    picker->Advance(*controller);
    EXPECT_EQ(selection.GetSelection()[0].id, other->GetUuid());
    picker->status = ViewportPickStatus::UNSUPPORTED;
    click();
    picker->Advance(*controller);
    EXPECT_EQ(selection.GetSelection()[0].id, other->GetUuid());
    EXPECT_FALSE(controller->Error().empty());
    for (int cancellation = 0; cancellation < 8; ++cancellation) {
        picker->status = ViewportPickStatus::PENDING;
        click();
        picker->Advance(*controller);
        const auto queries = picker->queries;
        switch (cancellation) {
        case 0: controller->Cancel(); break;
        case 1: controller->SetTool(GizmoTool::ROTATE); break;
        case 2: controller->camera.yaw += .1f; break;
        case 3: viewport.Arrange({20, 20, 820, 620}); break;
        case 4: controller->SetPickSurface(8); break;
        case 5: controller->SetEditing(false); break;
        case 6: controller->Bind(nullptr); break;
        case 7: picker->Advance(*controller, 2); break;
        }
        picker->status = ViewportPickStatus::MISS;
        picker->Advance(*controller);
        EXPECT_EQ(picker->queries, queries);
        EXPECT_EQ(selection.GetSelection()[0].id, other->GetUuid());
        controller->Bind(&document);
        controller->SetEditing(true);
        controller->SetTool(GizmoTool::SELECT);
        viewport.Arrange({10, 20, 810, 620});
        picker->Advance(*controller);
    }
}

TEST_F(SceneViewportTest, MatchingPointerTravelDpiAndNavigationPriority)
{
    auto picker    = std::make_shared<PreparedPicker>();
    picker->status = ViewportPickStatus::MISS;
    controller->SetPicker(picker);
    controller->SetTool(GizmoTool::SELECT);
    SceneViewport viewport(controller);
    viewport.Arrange({0, 0, 800, 600});
    viewport.OnPointerEvent(Pointer(ui::UIPointerAction::DOWN, 400, 300));
    auto foreign      = Pointer(ui::UIPointerAction::UP, 400, 300);
    foreign.pointerId = 42;
    viewport.OnPointerEvent(foreign);
    EXPECT_TRUE(controller->IsClickCandidate());
    SetDefaultUiTheme(MakeDarkTheme(2));
    viewport.OnPointerEvent(Pointer(ui::UIPointerAction::UP, 400, 300));
    picker->Advance(*controller);
    EXPECT_EQ(picker->queries, 0u);
    viewport.orbitHeld = [] { return true; };
    viewport.OnPointerEvent(Pointer(ui::UIPointerAction::DOWN, 400, 300));
    EXPECT_TRUE(controller->IsNavigating());
    EXPECT_FALSE(controller->IsClickCandidate());
    viewport.OnPointerEvent(Pointer(ui::UIPointerAction::UP, 400, 300));
    picker->Advance(*controller);
    EXPECT_EQ(picker->queries, 0u);
    EXPECT_FALSE(document.IsDirty());
}

TEST_F(SceneViewportTest, DeletedTargetCannotBeSelectedAndAllToolsPickOutsideHandles)
{
    auto  picker  = std::make_shared<PreparedPicker>();
    auto *other   = document.GetWorld()->CreateActor("Other");
    picker->actor = other->GetUuid();
    controller->SetPicker(picker);
    SceneViewport viewport(controller);
    viewport.Arrange({0, 0, 800, 600});
    const auto click = [&] {
        viewport.OnPointerEvent(Pointer(ui::UIPointerAction::DOWN, 40, 40));
        viewport.OnPointerEvent(Pointer(ui::UIPointerAction::UP, 40, 40));
    };
    for (const auto tool : {GizmoTool::SELECT, GizmoTool::TRANSLATE, GizmoTool::ROTATE, GizmoTool::SCALE}) {
        controller->SetTool(tool);
        selection.SetSelection({{SelectionType::ENTITY, actor->GetUuid()}});
        click();
        picker->Advance(*controller);
        EXPECT_EQ(selection.GetSelection()[0].id, other->GetUuid());
    }
    selection.SetSelection({{SelectionType::ENTITY, actor->GetUuid()}});
    picker->status = ViewportPickStatus::PENDING;
    click();
    picker->Advance(*controller);
    auto detached = document.GetWorld()->DetachFromWorld(other);
    detached.reset();
    picker->status = ViewportPickStatus::HIT;
    picker->Advance(*controller);
    EXPECT_EQ(selection.GetSelection()[0].id, actor->GetUuid());
    EXPECT_FALSE(document.IsDirty());
    EXPECT_EQ(commands.GetUndoCount(), 0u);
}

TEST_F(SceneViewportTest, CustomDrawingPickingCancellationAndOutsideRelease)
{
    auto picker            = std::make_shared<PreparedPicker>();
    picker->bounds.surface = true;
    controller->SetPicker(picker);
    controller->SetVisualProvider(std::make_shared<OffsetVisual>());
    SceneViewport viewport(controller);
    viewport.Arrange({100, 100, 900, 700});
    ui::UIPaintContext paint;
    paint.Begin({0, 0, 1000, 800});
    viewport.OnPaint(paint);
    ASSERT_EQ(paint.GetDrawData().vertices.size(), 6u);
    EXPECT_FLOAT_EQ(paint.GetDrawData().vertices.front().x, 120);
    EXPECT_FLOAT_EQ(paint.GetDrawData().commands.front().clip.left, 100);
    viewport.OnPointerEvent(Pointer(ui::UIPointerAction::DOWN, 150, 120));
    ASSERT_TRUE(controller->HasGesture());
    viewport.OnPointerEvent(Pointer(ui::UIPointerAction::MOVE, 170, 120));
    EXPECT_GT(actor->GetComponent<TransformComponent>()->GetLocalTranslation().x, 0);
    viewport.OnPointerCancel(1);
    EXPECT_TRUE(controller->HasGesture());
    viewport.OnPointerCancel(0);
    EXPECT_FALSE(controller->HasGesture());
    EXPECT_EQ(actor->GetComponent<TransformComponent>()->GetLocalTranslation().x, 0);
    viewport.OnPointerEvent(Pointer(ui::UIPointerAction::DOWN, 150, 120));
    viewport.OnPointerEvent(Pointer(ui::UIPointerAction::UP, 1000, 120));
    EXPECT_EQ(commands.GetUndoCount(), 1u);
    EXPECT_FALSE(controller->HasGesture());
    ASSERT_TRUE(commands.Undo());
    viewport.OnPointerEvent(Pointer(ui::UIPointerAction::DOWN, 150, 120));
    viewport.OnPointerEvent(Pointer(ui::UIPointerAction::MOVE, 170, 120));
    viewport.Arrange({110, 100, 910, 700});
    EXPECT_FALSE(controller->HasGesture());
    EXPECT_EQ(actor->GetComponent<TransformComponent>()->GetLocalTranslation().x, 0);
}

TEST_F(SceneViewportTest, WheelUsesNotchesAndSpeedWithoutEditingWorld)
{
    SceneViewport viewport(controller);
    viewport.Arrange({0, 0, 800, 600});
    auto event       = Pointer(ui::UIPointerAction::WHEEL, 400, 300);
    event.wheelDelta = 120;
    viewport.OnPointerEvent(event);
    EXPECT_FLOAT_EQ(controller->camera.position.z, 4.5f);
    controller->settings.speed = 10;
    viewport.OnPointerEvent(event);
    EXPECT_FLOAT_EQ(controller->camera.position.z, 3.5f);
    EXPECT_FALSE(document.IsDirty());
    EXPECT_EQ(commands.GetUndoCount(), 0u);
}

TEST_F(SceneViewportTest, FullCameraTurnKeepsEveryGizmoVertexInsideContent)
{
    SceneViewport viewport(controller);
    viewport.Arrange({100, 50, 900, 650});
    for (const auto tool : {GizmoTool::TRANSLATE, GizmoTool::ROTATE, GizmoTool::SCALE}) {
        controller->SetTool(tool);
        size_t visible = 0;
        for (int angle = 0; angle <= 720; ++angle) {
            controller->camera.yaw = float(angle) * 3.14159265f / 360.f;
            ui::UIPaintContext paint;
            paint.Begin({0, 0, 1000, 700});
            viewport.OnPaint(paint);
            visible += paint.GetDrawData().vertices.size();
            for (const auto &vertex : paint.GetDrawData().vertices) {
                ASSERT_TRUE(std::isfinite(vertex.x) && std::isfinite(vertex.y)) << angle;
                ASSERT_GE(vertex.x, 100) << angle;
                ASSERT_LE(vertex.x, 900) << angle;
                ASSERT_GE(vertex.y, 50) << angle;
                ASSERT_LE(vertex.y, 650) << angle;
            }
        }
        EXPECT_GT(visible, 0u);
    }
}

TEST_F(SceneViewportTest, MixedButtonsRetainCaptureUntilOwnerReleaseOnMainAndFloatingSurfaces)
{
    controller->SetVisualProvider(std::make_shared<OffsetVisual>());
    for (const uint32_t surface : {0u, 7u}) {
        LayoutModel layout;
        layout.SetDefault({"viewport"});
        PanelRegistry registry;
        registry.Register({"viewport", "Viewport"});
        EditorShell shell;
        shell.SetLayout(&layout);
        shell.SetPanelRegistry(&registry);
        shell.RegisterPanelView("viewport", [&]() { return std::make_unique<SceneViewport>(controller); });
        shell.Rebuild();
        shell.Layout(1000, 800);
        if (surface != 0) {
            FloatingPanel floating;
            floating.panelId = "viewport";
            shell.FloatPanelToSurface("viewport", surface, floating);
            ui::UIPaintContext paint;
            paint.Begin({0, 0, 600, 400});
            shell.PaintSurface(surface, paint, 600, 400);
        }
        const auto  bounds = shell.GetPanelView("viewport")->GetBounds();
        const float x = bounds.left + 50, y = bounds.top + 20;
        const auto  route = [&](ui::UIPointerAction action, float px, float py, uint32_t button) {
            shell.DispatchPointerToSurface(surface, Pointer(action, px, py, button));
        };
        for (const uint32_t owner : {0u, 1u, 2u}) {
            const uint32_t other = owner == 0 ? 1 : 0;
            route(ui::UIPointerAction::DOWN, x, y, owner);
            ASSERT_TRUE(controller->HasGesture());
            ASSERT_TRUE(shell.HasPointerCapture(surface));
            route(ui::UIPointerAction::DOWN, -20, -20, other);
            route(ui::UIPointerAction::UP, -20, -20, other);
            EXPECT_TRUE(shell.IsPanelInputActive("viewport"));
            EXPECT_TRUE(controller->HasGesture());
            EXPECT_TRUE(shell.HasPointerCapture(surface));
            route(ui::UIPointerAction::MOVE, x + 30, y, owner);
            route(ui::UIPointerAction::UP, -20, -20, owner);
            EXPECT_FALSE(controller->HasGesture());
            EXPECT_FALSE(shell.HasPointerCapture(surface));
            const auto yaw      = controller->camera.yaw;
            const auto position = controller->camera.position;
            route(ui::UIPointerAction::MOVE, x + 50, y + 50, 0);
            EXPECT_FLOAT_EQ(controller->camera.yaw, yaw);
            EXPECT_LT((controller->camera.position - position).Length(), 1.e-6f);
            if (owner == 0) {
                ASSERT_EQ(commands.GetUndoCount(), 1u);
                ASSERT_TRUE(commands.Undo());
            }
            controller->camera = {};
        }
    }
}

TEST_F(SceneViewportTest, ThemeScaleKeepsDrawingAndPickingAligned)
{
    for (const float scale : {1.f, 1.5f, 2.f}) {
        SetDefaultUiTheme(MakeDarkTheme(scale));
        SceneViewport viewport(controller);
        viewport.Arrange({0, 0, 800, 600});
        ui::UIPaintContext paint;
        paint.Begin({0, 0, 800, 600});
        viewport.OnPaint(paint);
        const float end      = 400 + GetDefaultUiTheme().metrics.gizmoLength;
        bool        foundTip = false;
        for (const auto &vertex : paint.GetDrawData().vertices)
            foundTip |= std::abs(vertex.x - end) < .01f && std::abs(vertex.y - 300) < .01f;
        EXPECT_TRUE(foundTip);
        viewport.OnPointerEvent(Pointer(ui::UIPointerAction::DOWN, end - 2, 300));
        EXPECT_EQ(controller->ActiveHandle(), GizmoHandle::X);
        controller->Cancel();
    }
    SetDefaultUiTheme(MakeDarkTheme());
}

TEST_F(SceneViewportTest, MainAndFloatingShortcutsRespectFocusAndNavigation)
{
    LayoutModel layout;
    layout.SetDefault({"viewport"});
    PanelRegistry registry;
    registry.Register({"viewport", "Viewport"});
    EditorShell shell;
    shell.SetLayout(&layout);
    shell.SetPanelRegistry(&registry);
    shell.RegisterPanelView("viewport", [&]() { return std::make_unique<SceneViewport>(controller); });
    int          invoked = 0;
    EditorAction action;
    action.id                  = "test.translate";
    action.accelerator         = "W";
    action.invoke              = [&]() { ++invoked; };
    action.acceleratorEligible = [&]() { return shell.IsPanelInputActive("viewport") && !controller->HasGesture(); };
    EditorActionRegistry::Get()->Add(action);
    shell.Rebuild();
    shell.Layout(1000, 800);
    ui::UIKeyEvent key;
    key.keyCode = 'W';
    key.action  = ui::UIKeyAction::DOWN;
    EXPECT_TRUE(ActionBarBuilder::MatchAccelerator(key).empty());
    const auto  bounds = shell.GetPanelView("viewport")->GetBounds();
    const float x = bounds.left + bounds.Width() * .5f, y = bounds.top + bounds.Height() * .5f;
    shell.DispatchPointer(Pointer(ui::UIPointerAction::DOWN, x, y));
    shell.DispatchPointer(Pointer(ui::UIPointerAction::UP, x, y));
    ASSERT_TRUE(shell.IsPanelInputActive("viewport"));
    shell.DispatchKey(key);
    EXPECT_EQ(invoked, 1);
    shell.DispatchPointer(Pointer(ui::UIPointerAction::DOWN, x, y, 1));
    shell.DispatchKey(key);
    controller->Tick(1);
    EXPECT_EQ(invoked, 1);
    EXPECT_LT(controller->camera.position.z, 5);
    controller->Cancel();
    shell.ResetInput();
    EXPECT_TRUE(ActionBarBuilder::MatchAccelerator(key).empty());
    FloatingPanel floating;
    floating.panelId = "viewport";
    floating.width   = 600;
    floating.height  = 400;
    shell.FloatPanelToSurface("viewport", 7, floating);
    ui::UIPaintContext paint;
    paint.Begin({0, 0, 600, 400});
    shell.PaintSurface(7, paint, 600, 400);
    const auto floatingBounds = shell.GetPanelView("viewport")->GetBounds();
    shell.DispatchPointerToSurface(7, Pointer(ui::UIPointerAction::DOWN, floatingBounds.left + 10, floatingBounds.top + 10));
    shell.DispatchPointerToSurface(7, Pointer(ui::UIPointerAction::UP, floatingBounds.left + 10, floatingBounds.top + 10));
    ASSERT_TRUE(shell.IsPanelInputActive("viewport"));
    shell.DispatchKeyToSurface(7, key);
    EXPECT_EQ(invoked, 2);
}

TEST_F(SceneViewportTest, SelectionFeedbackSurvivesSelectToolAndTracksEditingLifecycle)
{
    controller->SetTool(GizmoTool::SELECT);
    auto picker = std::make_shared<PreparedPicker>();
    controller->SetPicker(picker);
    GizmoGeometry geometry;
    controller->BuildSelectionGeometry({800, 600}, {}, geometry);
    ASSERT_EQ(geometry.size(), 1u);
    EXPECT_EQ(geometry.front().handle, GizmoHandle::NONE);
    EXPECT_TRUE(geometry.front().closed);
    picker->bounds.bounds = AABB{{-1, -1, -1}, {1, 1, 1}};
    geometry.clear();
    controller->BuildSelectionGeometry({800, 600}, {}, geometry);
    EXPECT_EQ(geometry.size(), 12u);
    picker->bounds.surface = true;
    geometry.clear();
    controller->BuildSelectionGeometry({800, 600}, {}, geometry);
    EXPECT_TRUE(geometry.empty());
    EXPECT_EQ(controller->SelectedActors(), (std::vector<Uuid>{actor->GetUuid()}));
    controller->SetEditing(false);
    EXPECT_TRUE(controller->SelectedActors().empty());
    controller->SetEditing(true);
    selection.Clear();
    EXPECT_TRUE(controller->SelectedActors().empty());
    EXPECT_FALSE(document.IsDirty());
    EXPECT_EQ(commands.GetUndoCount(), 0u);
    controller->Bind(nullptr);
    controller->BuildSelectionGeometry({800, 600}, {}, geometry);
    EXPECT_TRUE(geometry.empty());
}

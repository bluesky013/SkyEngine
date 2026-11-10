#include <curve/editor/CurveDocument.h>
#include <curve/editor/CurveEditor.h>
#include <editor/core/extension/EditorActionRegistry.h>
#include <editor/core/input/KeyModifiers.h>
#include <editor/ui/AssetViewProvider.h>
#include <editor/ui/UiTheme.h>
#include <editor/widgets/ActionBarBuilder.h>
#include <editor/widgets/CurveEditorWidget.h>
#include <editor/widgets/ReflectedFormView.h>
#include <gtest/gtest.h>
#include <ui/UIPaintContext.h>
#include <ui/widgets/ListView.h>

#include <algorithm>

using namespace sky;
using namespace sky::curve;
using namespace sky::curve::editor;

namespace {
    CurveSourceStorage MakeStorage(std::string &text)
    {
        return {[&text](std::string &out) {
                    out = text;
                    return FileOperationStatus{};
                },
                [&text](std::string_view value) {
                    text.assign(value);
                    return FileOperationStatus{};
                },
                {}};
    }
} // namespace

TEST(CurveDocument, LoadsEditsAndSaves)
{
    std::string text =
        R"({"version":1,"channels":1,"interpolation":"linear","preWrap":"clamp","postWrap":"clamp","keys":[{"time":0,"value":[0]},{"time":1,"value":[1]}]})";
    CurveDocument document(Uuid::Create(), MakeStorage(text));
    ASSERT_TRUE(document.Load());
    ASSERT_EQ(document.GetCurve().keys.size(), 2u);

    auto next            = document.GetCurve();
    next.keys[1].value.x = 4.f;
    ASSERT_TRUE(document.Apply("Edit key", next));
    EXPECT_EQ(document.GetCurve().keys[1].value.x, 4.f);

    ASSERT_TRUE(document.Commands().Undo());
    EXPECT_EQ(document.GetCurve().keys[1].value.x, 1.f);
    ASSERT_TRUE(document.Commands().Redo());
    EXPECT_EQ(document.GetCurve().keys[1].value.x, 4.f);

    ASSERT_TRUE(document.Save());
    CurveDocument reloaded(Uuid::Create(), MakeStorage(text));
    ASSERT_TRUE(reloaded.Load());
    EXPECT_EQ(reloaded.GetCurve().keys[1].value.x, 4.f);
}

TEST(CurveDocument, RejectsInvalidEdits)
{
    std::string text =
        R"({"version":1,"channels":1,"interpolation":"linear","preWrap":"clamp","postWrap":"clamp","keys":[{"time":0,"value":[0]},{"time":1,"value":[1]}]})";
    CurveDocument document(Uuid::Create(), MakeStorage(text));
    ASSERT_TRUE(document.Load());

    auto invalid         = document.GetCurve();
    invalid.keys[1].time = 0.f;
    EXPECT_FALSE(document.Apply("Invalid", invalid));
    EXPECT_FALSE(document.LastError().empty());
    EXPECT_EQ(document.GetCurve().keys[1].time, 1.f);

    auto empty = document.GetCurve();
    empty.keys.clear();
    EXPECT_FALSE(document.Apply("Empty", empty));
}

TEST(CurveEditorProvider, OpensAndRegistersCurveDocuments)
{
    std::string text =
        R"({"version":1,"channels":1,"interpolation":"linear","preWrap":"clamp","postWrap":"clamp","keys":[{"time":0,"value":[0]},{"time":1,"value":[1]}]})";
    auto provider = CreateCurveViewProvider();
    ASSERT_NE(provider, nullptr);
    ASSERT_TRUE(provider->UsesDocumentWindow());

    sky::editor::AssetViewProviderRegistry::Get()->Register("Curve", provider.get());
    EXPECT_EQ(sky::editor::AssetViewProviderRegistry::Get()->Find("Curve"), provider.get());
    sky::editor::AssetViewProviderRegistry::Get()->Unregister("Curve", provider.get());
    EXPECT_EQ(sky::editor::AssetViewProviderRegistry::Get()->Find("Curve"), nullptr);
}

namespace {
    using namespace sky::editor;
    using namespace sky::ui;

    struct CurveWidgetTest : testing::Test {
        std::string text =
            R"({"version":1,"channels":1,"interpolation":"linear","preWrap":"clamp","postWrap":"clamp","keys":[{"time":0,"value":[0]},{"time":0.5,"value":[1]},{"time":1,"value":[0]}]})";
        CurveDocument                        document{Uuid::Create(), MakeStorage(text)};
        std::unique_ptr<CurveEditorView>     view;
        std::unique_ptr<ScopedEditorActions> scope;

        void SetUp() override
        {
            ASSERT_TRUE(document.Load());
            const auto name = "curve-test-" + Uuid::Create().ToString();
            view            = std::make_unique<CurveEditorView>(document, nullptr, name);
            scope           = std::make_unique<ScopedEditorActions>(name);
            view->Arrange({0, 0, 1000, 700});
        }
        CurveEditorWidget &Widget()
        {
            return *static_cast<CurveEditorWidget *>(view->GetChildren().front().get());
        }
        UIElement &Graph()
        {
            return *Widget().GetChildren()[0];
        }
        ListView &List()
        {
            return *static_cast<ListView *>(Widget().GetChildren()[1].get());
        }
        void Tick()
        {
            view->Tick(0.f);
            view->Arrange({0, 0, 1000, 700});
        }
        void Pointer(UIPointerAction action, UIPoint point, uint32_t button = 0)
        {
            view->OnPointerEvent({action, 0, button, point.x, point.y});
        }
        std::vector<UIPoint> KeyPoints()
        {
            UIPaintContext paint;
            paint.Begin(view->GetBounds());
            view->Paint(paint);
            std::vector<UIPoint> points;
            const float          radius = GetDefaultUiTheme().metrics.selectionMarkerRadius;
            for (const auto &vertex : paint.GetDrawData().vertices) {
                if ((vertex.roundRadius != radius * .5f && vertex.roundRadius != radius * .75f) || vertex.roundHalfX != vertex.roundRadius ||
                    vertex.roundHalfY != vertex.roundRadius)
                    continue;
                const UIPoint point{vertex.roundCenterX, vertex.roundCenterY};
                if (Graph().GetBounds().Contains(point.x, point.y) &&
                    std::ranges::none_of(points, [&](const auto &existing) { return existing.x == point.x && existing.y == point.y; }))
                    points.push_back(point);
            }
            std::ranges::sort(points, {}, &UIPoint::x);
            return points;
        }
        void SelectOuterKeys()
        {
            const auto points = KeyPoints();
            ASSERT_EQ(points.size(), 3u);
            const float margin = GetDefaultUiTheme().metrics.gizmoHitRadius * 1.5f;
            const auto  bounds = Graph().GetBounds();
            // Start away from any key; the drag box includes the two lower endpoints.
            Pointer(UIPointerAction::DOWN, {bounds.left + 1, points[0].y - margin});
            Pointer(UIPointerAction::MOVE, {bounds.right - 1, points[2].y + margin});
            Pointer(UIPointerAction::UP, {bounds.right - 1, points[2].y + margin});
            EXPECT_EQ(Widget().Selection(), (std::vector<int>{0, 2}));
        }
    };
} // namespace

TEST_F(CurveWidgetTest, UndoDuringDragCancelsBeforeReplacingTheKeyArray)
{
    auto next = document.GetCurve();
    next.keys.insert(next.keys.begin() + 1, CurveKey{.time = .25f, .value = {.25f, 0, 0, 0}});
    ASSERT_TRUE(document.Apply("Add key", next));
    Tick();
    const auto points = KeyPoints();
    ASSERT_EQ(points.size(), 4u);
    Pointer(UIPointerAction::DOWN, points.back());
    Pointer(UIPointerAction::MOVE, {points.back().x - 10, points.back().y - 20});
    ASSERT_TRUE(view->Editing());
    ASSERT_TRUE(document.Commands().Undo());
    EXPECT_FALSE(view->Editing());
    Tick();
    Pointer(UIPointerAction::MOVE, {points.back().x - 20, points.back().y - 30});
    Pointer(UIPointerAction::UP, points.back());
    EXPECT_EQ(document.GetCurve().keys.size(), 3u);
    EXPECT_EQ(Widget().GetCurve().keys.size(), 3u);
    EXPECT_FLOAT_EQ(document.GetCurve().keys.back().value.x, 0.f);
    EXPECT_EQ(document.Commands().GetUndoCount(), 0u);
}

TEST_F(CurveWidgetTest, CancelKeepsSortedSelectionBeforeDeletingMultipleKeys)
{
    SelectOuterKeys();
    const auto points = KeyPoints();
    ASSERT_EQ(points.size(), 3u);
    Pointer(UIPointerAction::DOWN, points.front());
    Pointer(UIPointerAction::MOVE, {points.front().x, points.front().y - 10});
    view->Cancel();
    EXPECT_FALSE(view->Editing());
    EXPECT_EQ(Widget().PrimarySelection(), 0);
    EXPECT_EQ(Widget().Selection(), (std::vector<int>{0, 2}));
    const auto action = ActionBarBuilder::MatchAccelerator({46, UIKeyAction::DOWN, 0});
    EXPECT_EQ(action, "curve.removeKeys");
    ASSERT_TRUE(EditorActionRegistry::Get()->Invoke(action));
    ASSERT_EQ(document.GetCurve().keys.size(), 1u);
    EXPECT_FLOAT_EQ(document.GetCurve().keys.front().time, .5f);
    ASSERT_TRUE(document.Commands().Undo());
    EXPECT_EQ(document.GetCurve().keys.size(), 3u);
}

TEST_F(CurveWidgetTest, PointerReleaseOutsideTheGraphCommitsOnceAndEndsCapture)
{
    const auto points = KeyPoints();
    ASSERT_EQ(points.size(), 3u);
    Pointer(UIPointerAction::DOWN, points[1]);
    const UIPoint outside{points[1].x, Graph().GetBounds().bottom + 20};
    Pointer(UIPointerAction::MOVE, outside);
    Pointer(UIPointerAction::UP, outside);
    EXPECT_FALSE(view->Editing());
    EXPECT_FALSE(view->WantsPointerCapture());
    EXPECT_EQ(document.Commands().GetUndoCount(), 1u);
    const float committed = document.GetCurve().keys[1].value.x;
    EXPECT_NE(committed, 1.f);
    Tick();
    Pointer(UIPointerAction::MOVE, points[1]);
    EXPECT_FLOAT_EQ(document.GetCurve().keys[1].value.x, committed);
    EXPECT_EQ(document.Commands().GetUndoCount(), 1u);
}

TEST_F(CurveWidgetTest, ModelReplacementAndStructureCommandsEndAnActiveDrag)
{
    SelectOuterKeys();
    auto points = KeyPoints();
    ASSERT_EQ(points.size(), 3u);
    Pointer(UIPointerAction::DOWN, points.front());
    Widget().Execute(CurveEditorCommand::RemoveSelected);
    EXPECT_FALSE(view->Editing());
    ASSERT_EQ(document.GetCurve().keys.size(), 1u);
    Pointer(UIPointerAction::MOVE, points.back());
    Pointer(UIPointerAction::UP, points.back());
    ASSERT_TRUE(document.Commands().Undo());
    Tick();
    points = KeyPoints();
    ASSERT_EQ(points.size(), 3u);
    Pointer(UIPointerAction::DOWN, points.back());
    auto replacement = document.GetCurve();
    replacement.keys.pop_back();
    Widget().SetCurve(replacement);
    EXPECT_FALSE(view->Editing());
    Pointer(UIPointerAction::MOVE, points.back());
    EXPECT_EQ(Widget().GetCurve().keys.size(), 2u);
}

TEST_F(CurveWidgetTest, VirtualizedKeyListReachesTheLastRowWithoutScrollDrift)
{
    auto next = document.GetCurve();
    next.keys.clear();
    for (uint32_t i = 0; i < 100; ++i)
        next.keys.push_back({.time = float(i) / 99, .value = {float(i), 0, 0, 0}});
    ASSERT_TRUE(document.Apply("Many keys", std::move(next)));
    Tick();
    auto &list = List();
    EXPECT_LT(list.GetLiveItemCount(), list.GetItemCount());
    const auto bounds = list.GetBounds();
    view->OnPointerEvent({UIPointerAction::WHEEL, 0, 0, bounds.left + 10, bounds.top + 10, -1000});
    Tick();
    ASSERT_TRUE(list.IsItemLive(99));
    const auto scroll = list.GetScrollY();
    for (int frame = 0; frame < 3; ++frame) {
        Tick();
        KeyPoints();
        EXPECT_FLOAT_EQ(list.GetScrollY(), scroll);
        EXPECT_TRUE(list.IsItemLive(99));
    }
    const float y = bounds.top + list.GetItemHeight() * 99 - scroll + list.GetItemHeight() * .5f;
    Pointer(UIPointerAction::DOWN, {bounds.left + 10, y});
    Pointer(UIPointerAction::UP, {bounds.left + 10, y});
    EXPECT_EQ(Widget().PrimarySelection(), 99);
}

TEST_F(CurveWidgetTest, ScopedActionsExposeShortcutsAndSharedSnapState)
{
    for (const auto &command : CurveEditorCommands()) {
        const auto *action = EditorActionRegistry::Get()->Find(command.id);
        ASSERT_NE(action, nullptr);
        EXPECT_EQ(action->accelerator, command.accelerator);
    }
    const auto id = ActionBarBuilder::MatchAccelerator({'S', UIKeyAction::DOWN, 0});
    ASSERT_EQ(id, "curve.toggleSnap");
    const auto snapshot = EditorActionRegistry::Get()->GetSnapshot(id);
    EXPECT_TRUE(EditorActionRegistry::Get()->ResolveState(snapshot).checked);
    EXPECT_TRUE(EditorActionRegistry::Get()->Invoke(snapshot));
    EXPECT_FALSE(EditorActionRegistry::Get()->ResolveState(snapshot).checked);
    EXPECT_TRUE(ActionBarBuilder::MatchAccelerator({'S', UIKeyAction::DOWN, kModCtrl}).empty());
}

TEST_F(CurveWidgetTest, PropertyTextEditingKeepsDeleteOutOfCurveCommands)
{
    const auto points = KeyPoints();
    ASSERT_EQ(points.size(), 3u);
    Pointer(UIPointerAction::DOWN, points[1]);
    Pointer(UIPointerAction::UP, points[1]);
    EXPECT_EQ(document.Commands().GetUndoCount(), 0u);
    auto      &form   = *static_cast<ReflectedFormView *>(Widget().GetChildren()[2].get());
    auto      &fields = form.Form().GetSections().front().fields;
    const auto field  = std::ranges::find(fields, std::string("time"), &PropertyField::path);
    ASSERT_NE(field, fields.end());
    form.BeginTextEdit(*field, -1, PropertyEditorKind::Float, "0.5", [](const std::string &) { return true; });
    EXPECT_TRUE(view->Editing());
    EXPECT_TRUE(ActionBarBuilder::MatchAccelerator({46, UIKeyAction::DOWN, 0}).empty());
    EXPECT_FALSE(EditorActionRegistry::Get()->ResolveState(EditorActionRegistry::Get()->GetSnapshot("curve.removeKeys")).enabled);
    view->OnKeyEvent({46, UIKeyAction::DOWN, 0});
    EXPECT_EQ(document.GetCurve().keys.size(), 3u);
    view->Cancel();
    EXPECT_EQ(ActionBarBuilder::MatchAccelerator({46, UIKeyAction::DOWN, 0}), "curve.removeKeys");
}

TEST_F(CurveWidgetTest, ClosingTheViewWithdrawsActionsAndHistoryCallbacks)
{
    auto next            = document.GetCurve();
    next.keys[1].value.x = 2.f;
    ASSERT_TRUE(document.Apply("Edit", next));
    Tick();
    view.reset();
    EXPECT_EQ(EditorActionRegistry::Get()->Find("curve.removeKeys"), nullptr);
    ASSERT_TRUE(document.Commands().Undo());
    EXPECT_FLOAT_EQ(document.GetCurve().keys[1].value.x, 1.f);
}

TEST_F(CurveWidgetTest, OwnPropertyCommitKeepsTheActiveBindingUntilAnExternalEdit)
{
    const auto points = KeyPoints();
    ASSERT_EQ(points.size(), 3u);
    Pointer(UIPointerAction::DOWN, points[1]);
    Pointer(UIPointerAction::UP, points[1]);
    auto      &form   = *static_cast<ReflectedFormView *>(Widget().GetChildren()[2].get());
    auto      &fields = form.Form().GetSections().front().fields;
    const auto field  = std::ranges::find(fields, std::string("time"), &PropertyField::path);
    ASSERT_NE(field, fields.end());
    form.BeginTextEdit(*field, -1, PropertyEditorKind::Float, "0.5", [](const std::string &) { return true; });
    const auto binding = form.BindingLifetime();
    ASSERT_TRUE(form.Form().Edit(*field, Any(.6f), document.Commands()));
    Tick();
    EXPECT_TRUE(view->Editing());
    EXPECT_FALSE(binding.expired());
    EXPECT_FLOAT_EQ(document.GetCurve().keys[1].time, .6f);
    auto next         = document.GetCurve();
    next.keys[1].time = .7f;
    ASSERT_TRUE(document.Apply("External property edit", next));
    Tick();
    EXPECT_FALSE(view->Editing());
    EXPECT_TRUE(binding.expired());
    EXPECT_FLOAT_EQ(Widget().GetCurve().keys[1].time, .7f);
}

TEST_F(CurveWidgetTest, ReloadDuringDragReplacesTheSnapshotBeforeTheNextPointerEvent)
{
    const auto points = KeyPoints();
    ASSERT_EQ(points.size(), 3u);
    Pointer(UIPointerAction::DOWN, points.back());
    Pointer(UIPointerAction::MOVE, {points.back().x - 20, points.back().y - 20});
    ASSERT_TRUE(view->Editing());
    text =
        R"({"version":1,"channels":1,"interpolation":"linear","preWrap":"clamp","postWrap":"clamp","keys":[{"time":0,"value":[7]},{"time":1,"value":[8]}]})";
    ASSERT_TRUE(document.Load());
    Pointer(UIPointerAction::MOVE, points.back());
    Pointer(UIPointerAction::UP, points.back());
    EXPECT_FALSE(view->Editing());
    EXPECT_FALSE(document.Commands().CanUndo());
    ASSERT_EQ(Widget().GetCurve().keys.size(), 2u);
    EXPECT_FLOAT_EQ(Widget().GetCurve().keys.back().value.x, 8.f);
    EXPECT_FLOAT_EQ(document.GetCurve().keys.back().value.x, 8.f);
}

TEST_F(CurveWidgetTest, ExternalEditDuringDragCancelsWhileOwnCommitsRemainSynchronized)
{
    const auto points = KeyPoints();
    ASSERT_EQ(points.size(), 3u);
    Pointer(UIPointerAction::DOWN, points[1]);
    Pointer(UIPointerAction::MOVE, {points[1].x, points[1].y + 20});
    auto next            = document.GetCurve();
    next.keys[1].value.x = 5.f;
    ASSERT_TRUE(document.Apply("External change", next));
    Tick();
    EXPECT_FALSE(view->Editing());
    Pointer(UIPointerAction::UP, points[1]);
    EXPECT_FLOAT_EQ(document.GetCurve().keys[1].value.x, 5.f);
    EXPECT_EQ(document.Commands().GetUndoCount(), 1u);

    Widget().Execute(CurveEditorCommand::SetStep);
    EXPECT_EQ(document.Commands().GetUndoCount(), 2u);
    const auto updated = KeyPoints();
    ASSERT_GE(updated.size(), 3u);
    Pointer(UIPointerAction::DOWN, updated.front());
    ASSERT_TRUE(view->Editing());
    Tick();
    EXPECT_TRUE(view->Editing());
    view->Cancel();
}

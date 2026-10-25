#include <array>
#include <editor/widgets/GraphCanvas.h>
#include <gtest/gtest.h>
#include <ui/UIPaintContext.h>

using namespace sky;
using namespace sky::editor;
using namespace sky::ui;

namespace {
    struct CanvasFixture : testing::Test {
        GraphCanvas canvas{nullptr};
        Uuid        a = Uuid::Create(), b = Uuid::Create();
        void        SetUp() override
        {
            ASSERT_TRUE(canvas.SetModel(
                {{{a, "Source", {30, 30}, {{0, "Pose", Name("pose"), true}}}, {b, "Result", {350, 30}, {{1, "Input", Name("pose"), false}}}}, {}}));
            canvas.Arrange({10, 20, 1000, 700});
        }
        void Pointer(UIPointerAction action, UIPoint graph, uint32_t button = 0)
        {
            const auto screen = canvas.ToScreen(graph);
            canvas.OnPointerEvent({action, 5, button, screen.x, screen.y});
        }
    };
} // namespace
TEST_F(CanvasFixture, DragCommitsOnceAndCancellationRestoresTheStartingPositions)
{
    uint32_t                         commits = 0;
    std::vector<GraphCanvasPosition> result;
    canvas.SetCallbacks({.moved = [&](auto value) {
        ++commits;
        result = std::move(value);
    }});
    canvas.SetSelection(std::array{a, b});
    Pointer(UIPointerAction::DOWN, {50, 40});
    Pointer(UIPointerAction::MOVE, {70, 60});
    Pointer(UIPointerAction::MOVE, {90, 80});
    EXPECT_EQ(commits, 0u);
    EXPECT_FLOAT_EQ(canvas.Model().nodes[0].position.x, 70);
    canvas.OnPointerCancel(5);
    EXPECT_FLOAT_EQ(canvas.Model().nodes[0].position.x, 30);
    EXPECT_FLOAT_EQ(canvas.Model().nodes[1].position.x, 350);
    Pointer(UIPointerAction::DOWN, {50, 40});
    Pointer(UIPointerAction::MOVE, {70, 60});
    Pointer(UIPointerAction::UP, {90, 80});
    EXPECT_EQ(commits, 1u);
    ASSERT_EQ(result.size(), 2u);
    EXPECT_FLOAT_EQ(result[0].position.x, 70);
    EXPECT_FLOAT_EQ(result[1].position.x, 390);
}
TEST_F(CanvasFixture, ZoomKeepsCursorAnchorAndPanCancellationDoesNotCommit)
{
    uint32_t commits = 0;
    canvas.SetCallbacks({.viewChanged = [&](auto) { ++commits; }});
    const auto before = canvas.ToGraph({200, 150});
    canvas.OnPointerEvent({UIPointerAction::WHEEL, 5, 0, 200, 150, 2});
    const auto after = canvas.ToGraph({200, 150});
    EXPECT_NEAR(before.x, after.x, .0001f);
    EXPECT_NEAR(before.y, after.y, .0001f);
    const auto view = canvas.View();
    canvas.OnPointerEvent({UIPointerAction::DOWN, 5, 2, 200, 150});
    canvas.OnPointerEvent({UIPointerAction::MOVE, 5, 2, 230, 190});
    canvas.OnPointerCancel(5);
    EXPECT_FLOAT_EQ(canvas.View().pan.x, view.pan.x);
    EXPECT_FLOAT_EQ(canvas.View().pan.y, view.pan.y);
    EXPECT_EQ(commits, 1u);
}
TEST_F(CanvasFixture, ConnectionsRequireOppositeDirectionsAndMatchingTypes)
{
    std::vector<GraphCanvasLink> links;
    canvas.SetCallbacks({.connected = [&](auto link) { links.push_back(link); }});
    const auto   &m = GetDefaultUiTheme().metrics;
    const UIPoint output{30 + m.fieldWidth, 30 + m.panelHeaderHeight + m.rowHeight * .5f};
    const UIPoint input{350, output.y};
    const auto    hit = canvas.HitPin(canvas.ToScreen(output));
    ASSERT_TRUE(hit);
    EXPECT_EQ(hit->node, a);
    Pointer(UIPointerAction::DOWN, output);
    Pointer(UIPointerAction::UP, input);
    ASSERT_EQ(links.size(), 1u);
    EXPECT_EQ(links[0].from.node, a);
    EXPECT_EQ(links[0].to.node, b);
    auto model                  = canvas.Model();
    model.nodes[1].pins[0].type = Name("number");
    ASSERT_TRUE(canvas.SetModel(std::move(model)));
    Pointer(UIPointerAction::DOWN, input);
    Pointer(UIPointerAction::UP, output);
    EXPECT_EQ(links.size(), 1u);
    Pointer(UIPointerAction::DOWN, output);
    Pointer(UIPointerAction::UP, output);
    EXPECT_EQ(links.size(), 1u);
}
TEST_F(CanvasFixture, BoxSelectionModelReplacementAndThemePaintUseOwnedData)
{
    Pointer(UIPointerAction::DOWN, {0, 0});
    Pointer(UIPointerAction::UP, {600, 200});
    EXPECT_EQ(canvas.Selection().size(), 2u);
    canvas.SetSelection(canvas.Selection());
    EXPECT_EQ(canvas.Selection().size(), 2u);
    Pointer(UIPointerAction::DOWN, {50, 40});
    Pointer(UIPointerAction::MOVE, {75, 60});
    auto model = canvas.Model();
    model.nodes.erase(model.nodes.begin());
    ASSERT_TRUE(canvas.SetModel(std::move(model)));
    Pointer(UIPointerAction::UP, {90, 80});
    ASSERT_EQ(canvas.Selection().size(), 1u);
    EXPECT_EQ(canvas.Selection()[0], b);
    UIPaintContext context;
    context.Begin(canvas.GetBounds());
    canvas.Paint(context);
    EXPECT_FALSE(context.GetDrawData().vertices.empty());
}

TEST_F(CanvasFixture, DpiScalingPreservesGraphCoordinatesAndFramingKeepsNodesVisible)
{
    struct RestoreTheme {
        UiTheme previous = GetDefaultUiTheme();
        ~RestoreTheme()
        {
            SetDefaultUiTheme(previous);
        }
    } restore;
    const auto original = canvas.ToScreen({350, 30});
    SetDefaultUiTheme(MakeDarkTheme(2.f));
    const auto scaled = canvas.ToScreen({350, 30});
    EXPECT_FLOAT_EQ(scaled.x - canvas.GetBounds().left, 2 * (original.x - canvas.GetBounds().left));
    const auto gap = canvas.ToScreen({300, 45});
    EXPECT_FALSE(canvas.HitNode(gap));
    EXPECT_EQ(canvas.HitNode(canvas.ToScreen({355, 45})), b);
    const auto anchor = canvas.ToGraph({400, 200});
    canvas.OnPointerEvent({UIPointerAction::WHEEL, 5, 0, 400, 200, 1});
    const auto after = canvas.ToGraph({400, 200});
    EXPECT_NEAR(anchor.x, after.x, .001f);
    EXPECT_NEAR(anchor.y, after.y, .001f);
    ASSERT_TRUE(canvas.FrameAll());
    EXPECT_TRUE(canvas.GetBounds().Contains(canvas.ToScreen({30, 30}).x, canvas.ToScreen({30, 30}).y));
    const auto last = canvas.ToScreen({570, 90});
    EXPECT_TRUE(canvas.GetBounds().Contains(last.x, last.y));
    EXPECT_FLOAT_EQ(canvas.Model().nodes.front().position.x, 30);
}

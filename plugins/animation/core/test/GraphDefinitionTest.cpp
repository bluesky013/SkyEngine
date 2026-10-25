#include <algorithm>
#include <animation/graph/GraphDefinition.h>
#include <gtest/gtest.h>
#include <limits>

using namespace sky;
using namespace sky::animation;

namespace {
    GraphDefinition PoseGraph()
    {
        GraphDefinition graph;
        graph.document = Uuid::Create();
        graph.skeleton = Uuid::Create();
        GraphNode clip;
        clip.id   = Uuid::Create();
        clip.clip = Uuid::Create();
        GraphNode output;
        output.id    = Uuid::Create();
        output.kind  = GraphNodeKind::Output;
        graph.nodes  = {clip, output};
        graph.output = output.id;
        graph.links  = {{clip.id, GraphPin::Pose, output.id, GraphPin::InputA}};
        return graph;
    }
    GraphDefinition StateGraph()
    {
        auto graph = PoseGraph();
        auto other = PoseGraph();
        graph.nodes.insert(graph.nodes.end(), other.nodes.begin(), other.nodes.end());
        graph.links.insert(graph.links.end(), other.links.begin(), other.links.end());
        graph.states     = {{Uuid::Create(), "Idle", graph.output}, {Uuid::Create(), "Walk", other.output}};
        graph.entryState = graph.states[0].id;
        graph.output     = {};
        GraphTransition transition;
        transition.id   = Uuid::Create();
        transition.from = graph.states[0].id;
        transition.to   = graph.states[1].id;
        graph.transitions.push_back(transition);
        transition.id = Uuid::Create();
        std::swap(transition.from, transition.to);
        graph.transitions.push_back(transition);
        return graph;
    }
    bool HasMessage(const GraphValidation &result, const char *text)
    {
        return std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
                           [&](const auto &item) { return item.message.find(text) != std::string::npos; });
    }
} // namespace

TEST(GraphDefinition, DraftsMayBeIncompleteButExecutableGraphsNeedOneOutput)
{
    GraphSource source;
    source.definition.document = Uuid::Create();
    EXPECT_TRUE(ValidateGraphSource(source));
    EXPECT_FALSE(ValidateGraph(source.definition));
    auto graph = PoseGraph();
    EXPECT_TRUE(ValidateGraph(graph));
    graph.output = graph.nodes[0].id;
    EXPECT_TRUE(ValidateGraph(graph, GraphValidationMode::Draft));
    EXPECT_TRUE(HasMessage(ValidateGraph(graph), "Root must reference"));
    graph = PoseGraph();
    graph.nodes.push_back({Uuid::Create(), GraphNodeKind::Output});
    EXPECT_FALSE(ValidateGraph(graph));
}

TEST(GraphDefinition, RejectsDuplicatesUnknownNodesWrongPinsAndNonfiniteValues)
{
    auto graph        = PoseGraph();
    graph.nodes[1].id = graph.nodes[0].id;
    EXPECT_TRUE(HasMessage(ValidateGraph(graph), "duplicated"));
    graph               = PoseGraph();
    graph.nodes[0].kind = static_cast<GraphNodeKind>(9);
    EXPECT_FALSE(ValidateGraph(graph, GraphValidationMode::Draft));
    graph                = PoseGraph();
    graph.links[0].input = static_cast<GraphPin>(9);
    EXPECT_TRUE(HasMessage(ValidateGraph(graph), "incompatible types"));
    graph               = PoseGraph();
    graph.nodes[0].rate = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(ValidateGraph(graph, GraphValidationMode::Draft));
    graph               = PoseGraph();
    graph.links[0].from = Uuid::Create();
    EXPECT_TRUE(HasMessage(ValidateGraph(graph), "unknown node"));
}

TEST(GraphDefinition, RejectsPoseCyclesAndSharedStatefulSubtrees)
{
    auto      graph = PoseGraph();
    GraphNode blend;
    blend.id   = Uuid::Create();
    blend.kind = GraphNodeKind::Blend2;
    graph.nodes.push_back(blend);
    graph.links = {{graph.nodes[0].id, GraphPin::Pose, blend.id, GraphPin::InputA},
                   {graph.nodes[0].id, GraphPin::Pose, blend.id, GraphPin::InputB},
                   {blend.id, GraphPin::Pose, graph.output, GraphPin::InputA}};
    EXPECT_TRUE(HasMessage(ValidateGraph(graph), "multiple consumers"));
    graph.links[1].from = blend.id;
    EXPECT_TRUE(HasMessage(ValidateGraph(graph), "cycle"));
}

TEST(GraphDefinition, StateCyclesAreLegalButSelfTransitionsAndSharedStateTreesAreNot)
{
    auto graph = StateGraph();
    ASSERT_TRUE(ValidateGraph(graph));
    graph.transitions[0].to = graph.transitions[0].from;
    EXPECT_TRUE(HasMessage(ValidateGraph(graph), "self-transition"));
    graph                  = StateGraph();
    graph.states[1].output = graph.states[0].output;
    EXPECT_TRUE(HasMessage(ValidateGraph(graph), "disjoint"));
    graph            = StateGraph();
    graph.entryState = Uuid::Create();
    EXPECT_TRUE(HasMessage(ValidateGraph(graph), "entry state"));
}

TEST(GraphDefinition, ExitClocksAndConditionTypesAreExplicit)
{
    auto  graph            = StateGraph();
    auto &transition       = graph.transitions[0];
    transition.hasExitTime = true;
    transition.exitTime    = 0.75f;
    EXPECT_TRUE(HasMessage(ValidateGraph(graph), "explicit Clip clock"));
    transition.timeSourceNode = graph.nodes[0].id;
    EXPECT_TRUE(ValidateGraph(graph));
    transition.timeSourceNode = graph.nodes[2].id;
    EXPECT_FALSE(ValidateGraph(graph));
    transition.timeSourceNode = graph.nodes[0].id;
    graph.parameters          = {{Uuid::Create(), "walking", GraphParameterType::Bool, false}};
    transition.conditions     = {{graph.parameters[0].id, GraphComparison::Equal, true}};
    EXPECT_TRUE(ValidateGraph(graph));
    transition.conditions[0].comparison = GraphComparison::Greater;
    EXPECT_TRUE(HasMessage(ValidateGraph(graph), "Boolean conditions"));
    transition.conditions[0].comparison = GraphComparison::Equal;
    transition.conditions[0].value      = 1.f;
    EXPECT_TRUE(HasMessage(ValidateGraph(graph), "wrong type"));
}

TEST(GraphDefinition, NormalizesOrderDependenciesAndPreservesPriorOutputOnFailure)
{
    auto graph                    = StateGraph();
    graph.nodes[2].clip           = graph.nodes[0].clip;
    graph.transitions[0].priority = 1;
    graph.transitions[1].priority = 3;
    CookedGraph first;
    ASSERT_TRUE(CompileGraphDefinition(graph, first));
    EXPECT_EQ(first.dependencies.size(), 2u);
    EXPECT_EQ(first.definition.transitions[0].priority, 3);
    std::reverse(graph.nodes.begin(), graph.nodes.end());
    std::reverse(graph.links.begin(), graph.links.end());
    std::reverse(graph.states.begin(), graph.states.end());
    std::reverse(graph.transitions.begin(), graph.transitions.end());
    CookedGraph second;
    ASSERT_TRUE(CompileGraphDefinition(graph, second));
    EXPECT_EQ(first.topologicalNodes, second.topologicalNodes);
    EXPECT_EQ(first.dependencies, second.dependencies);
    for (size_t i = 0; i < first.definition.nodes.size(); ++i) {
        EXPECT_EQ(first.definition.nodes[i].id, second.definition.nodes[i].id);
    }
    const auto prior = second.definition.document;
    graph.entryState = {};
    EXPECT_FALSE(CompileGraphDefinition(graph, second));
    EXPECT_EQ(second.definition.document, prior);
}

TEST(GraphDefinition, LayoutIsValidatedSeparatelyFromCookedData)
{
    GraphSource source;
    source.definition   = PoseGraph();
    source.editor.panX  = 12.f;
    source.editor.nodes = {{source.definition.nodes[0].id, 100.f, 50.f}};
    EXPECT_TRUE(ValidateGraphSource(source));
    source.editor.zoom = 0.f;
    EXPECT_FALSE(ValidateGraphSource(source));
    CookedGraph cooked;
    EXPECT_TRUE(CompileGraphDefinition(source.definition, cooked));
    EXPECT_EQ(cooked.definition.document, source.definition.document);
}

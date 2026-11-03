#include <animation/assets/GraphAsset.h>
#include <core/archive/MemoryStreamArchive.h>
#include <gtest/gtest.h>
#include <limits>

using namespace sky;
using namespace sky::animation;

namespace {
    GraphSource Source()
    {
        GraphSource source;
        auto       &graph = source.definition;
        graph.document    = Uuid::Create();
        graph.skeleton    = Uuid::Create();
        GraphNode clip;
        clip.id   = Uuid::Create();
        clip.clip = Uuid::Create();
        GraphNode output;
        output.id        = Uuid::Create();
        output.kind      = GraphNodeKind::Output;
        graph.output     = output.id;
        graph.nodes      = {clip, output};
        graph.links      = {{clip.id, GraphPin::Pose, output.id, GraphPin::InputA}};
        graph.parameters = {{Uuid::Create(), "speed", GraphParameterType::Float, 0.5f}, {Uuid::Create(), "moving", GraphParameterType::Bool, false}};
        source.editor.nodes = {{clip.id, 12.f, 24.f}};
        source.editor.panX  = 17.f;
        return source;
    }
} // namespace

TEST(GraphAsset, SourceRoundTripPreservesIdentityTypesAndIndependentLayout)
{
    const auto  source = Source();
    std::string text;
    ASSERT_TRUE(WriteGraphSource(source, text));
    GraphSource decoded;
    ASSERT_TRUE(ParseGraphSource(text, decoded));
    EXPECT_EQ(decoded.definition.document, source.definition.document);
    EXPECT_EQ(decoded.definition.nodes[0].id, source.definition.nodes[0].id);
    EXPECT_TRUE(std::holds_alternative<float>(decoded.definition.parameters[0].value));
    EXPECT_TRUE(std::holds_alternative<bool>(decoded.definition.parameters[1].value));
    EXPECT_FLOAT_EQ(decoded.editor.panX, 17.f);
    std::string repeated;
    ASSERT_TRUE(WriteGraphSource(decoded, repeated));
    EXPECT_EQ(repeated, text);
    const auto prior = decoded.definition.document;
    EXPECT_FALSE(ParseGraphSource("[]", decoded));
    EXPECT_EQ(decoded.definition.document, prior);
    EXPECT_FALSE(ParseGraphSource("{\"version\":9}", decoded));
}

TEST(GraphAsset, LegacyEmptySourceMigratesToEditableDraftWithoutExecutableOutput)
{
    GraphSource source;
    const auto  owner = Uuid::Create();
    ASSERT_TRUE(ParseGraphSource("{\"version\":1}", source, owner));
    EXPECT_EQ(source.definition.document, owner);
    EXPECT_EQ(source.definition.version, GRAPH_VERSION);
    EXPECT_TRUE(ValidateGraphSource(source));
    EXPECT_FALSE(ValidateGraph(source.definition));
    std::string text;
    EXPECT_TRUE(WriteGraphSource(source, text));
    EXPECT_FALSE(ParseGraphSource("{\"version\":1,\"unexpected\":true}", source, owner));
}

TEST(GraphAsset, ProductContainsCompleteDefinitionAndRejectsLegacyTruncationAndBadTopology)
{
    GraphAssetData data;
    ASSERT_TRUE(CompileGraphDefinition(Source().definition, data.graph));
    OMemoryArchive      output;
    BinaryOutputArchive writer(output);
    data.Save(writer);
    ASSERT_TRUE(writer.IsValid());
    IMemoryArchive     input(output.Data(), output.Size());
    BinaryInputArchive reader(input);
    GraphAssetData     decoded;
    decoded.Load(reader);
    ASSERT_TRUE(decoded.IsLoadValid());
    EXPECT_EQ(decoded.graph.dependencies, data.graph.dependencies);
    EXPECT_EQ(decoded.graph.topologicalNodes, data.graph.topologicalNodes);
    EXPECT_EQ(decoded.graph.definition.document, data.graph.definition.document);
    EXPECT_EQ(decoded.graph.definition.parameters.size(), 2u);
    IMemoryArchive     shortInput(output.Data(), output.Size() - 2);
    BinaryInputArchive shortReader(shortInput);
    decoded.Load(shortReader);
    EXPECT_FALSE(decoded.IsLoadValid());
    EXPECT_TRUE(decoded.graph.definition.nodes.empty());
    const uint32_t     legacy = 1;
    IMemoryArchive     oldInput(reinterpret_cast<const char *>(&legacy), sizeof(legacy));
    BinaryInputArchive oldReader(oldInput);
    decoded.Load(oldReader);
    EXPECT_FALSE(decoded.IsLoadValid());
    EXPECT_NE(decoded.validation.diagnostics[0].message.find("recook"), std::string::npos);
    data.graph.topologicalNodes[0] = 9999;
    OMemoryArchive      corrupt;
    BinaryOutputArchive badWriter(corrupt);
    data.Save(badWriter);
    IMemoryArchive     badInput(corrupt.Data(), corrupt.Size());
    BinaryInputArchive badReader(badInput);
    decoded.Load(badReader);
    EXPECT_FALSE(decoded.IsLoadValid());
}

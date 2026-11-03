#include "animation/editor/GraphDocument.h"
#include <array>
#include <gtest/gtest.h>

using namespace sky;
using namespace sky::animation;
using namespace sky::animation::editor;

namespace {
    struct Storage {
        std::string          text   = "{}";
        uint32_t             writes = 0, cooks = 0;
        bool                 fail = false;
        GraphDocumentStorage Bind()
        {
            return {[this](std::string &out) -> FileOperationStatus {
                        out = text;
                        return {};
                    },
                    [this](std::string_view value) -> FileOperationStatus {
                        ++writes;
                        if (fail)
                            return {FileOperationError::IOError, "Injected write failure"};
                        text = value;
                        return {};
                    },
                    [this](const Uuid &) { ++cooks; }};
        }
    };
} // namespace

TEST(AnimationGraphDocument, DraftSaveIsDeferredAndFailurePreservesDirtyWorkAndSource)
{
    Storage       storage;
    const auto    asset = Uuid::Create();
    GraphDocument document(asset, storage.Bind());
    ASSERT_TRUE(document.Load());
    EXPECT_FALSE(document.IsDirty());
    EXPECT_FALSE(document.Diagnostics());
    const auto node = document.AddNode(GraphNodeKind::Clip, 20, 30);
    ASSERT_TRUE(node);
    document.RequestSave();
    document.RequestSave();
    EXPECT_EQ(storage.writes, 0u);
    EXPECT_TRUE(document.IsDirty());
    storage.fail = true;
    EXPECT_FALSE(document.FlushPendingSave());
    EXPECT_EQ(storage.text, "{}");
    EXPECT_EQ(storage.cooks, 0u);
    EXPECT_EQ(document.Source().definition.nodes.front().id, node);
    EXPECT_TRUE(document.IsDirty());
    storage.fail = false;
    document.RequestSave();
    ASSERT_TRUE(document.FlushPendingSave());
    EXPECT_FALSE(document.IsDirty());
    EXPECT_EQ(storage.cooks, 1u);
    EXPECT_FALSE(document.Diagnostics());
    GraphDocument reopened(asset, storage.Bind());
    ASSERT_TRUE(reopened.Load());
    EXPECT_EQ(reopened.Source().definition.nodes.front().id, node);
    EXPECT_EQ(reopened.Source().editor.nodes.front().x, 20.f);
}

TEST(AnimationGraphDocument, MoveAndCopyHaveOneUndoStepAndFreshStableIds)
{
    Storage       storage;
    GraphDocument document(Uuid::Create(), storage.Bind());
    ASSERT_TRUE(document.Load());
    const auto clip   = document.AddNode(GraphNodeKind::Clip, 0, 0);
    const auto output = document.AddNode(GraphNodeKind::Output, 100, 0);
    ASSERT_TRUE(document.Connect(clip, output, GraphPin::InputA));
    ASSERT_TRUE(document.Save());
    const auto count = document.Commands().GetUndoCount();
    ASSERT_TRUE(document.MoveNodes(std::array{GraphNodePosition{clip, 20, 30}, GraphNodePosition{output, 120, 30}}));
    EXPECT_EQ(document.Commands().GetUndoCount(), count + 1);
    EXPECT_TRUE(document.Commands().Undo());
    EXPECT_FALSE(document.IsDirty());
    EXPECT_FLOAT_EQ(document.Source().editor.nodes[0].x, 0.f);
    EXPECT_TRUE(document.Commands().Redo());
    const auto copies = document.CopyNodes(std::array{clip, output}, 50, 50);
    ASSERT_EQ(copies.size(), 2u);
    EXPECT_NE(copies[0], clip);
    EXPECT_NE(copies[1], output);
    ASSERT_EQ(document.Source().definition.links.size(), 2u);
    EXPECT_EQ(document.Source().definition.links.back().from, copies[0]);
    EXPECT_EQ(document.Source().definition.links.back().to, copies[1]);
    EXPECT_TRUE(document.Commands().Undo());
    EXPECT_EQ(document.Source().definition.nodes.size(), 2u);
    EXPECT_TRUE(document.Commands().Redo());
    EXPECT_EQ(document.Source().definition.nodes.back().id, copies[1]);
    EXPECT_FALSE(document.Connect(output, clip, GraphPin::InputA));
}

TEST(AnimationGraphDocument, StateAndParameterDeletionRemovesReferencesAndUndoRestoresThem)
{
    Storage       storage;
    GraphDocument document(Uuid::Create(), storage.Bind());
    ASSERT_TRUE(document.Load());
    const auto parameter = document.AddParameter("moving", GraphParameterType::Bool);
    const auto idle = document.AddState("Idle"), walk = document.AddState("Walk");
    const auto transition = document.AddTransition(idle, walk);
    ASSERT_TRUE(transition);
    EXPECT_FALSE(document.AddTransition(idle, idle));
    auto next = document.Source();
    next.definition.transitions.front().conditions.push_back({parameter, GraphComparison::Equal, true});
    ASSERT_TRUE(document.Apply("Set condition", std::move(next)));
    ASSERT_TRUE(document.Remove(std::array{parameter}));
    EXPECT_TRUE(document.Source().definition.transitions[0].conditions.empty());
    EXPECT_TRUE(document.Commands().Undo());
    EXPECT_EQ(document.Source().definition.transitions[0].conditions[0].parameter, parameter);
    ASSERT_TRUE(document.Remove(std::array{idle}));
    EXPECT_TRUE(document.Source().definition.transitions.empty());
    EXPECT_FALSE(document.Source().definition.entryState);
    EXPECT_TRUE(document.Commands().Undo());
    EXPECT_EQ(document.Source().definition.transitions[0].id, transition);
    next                                     = document.Source();
    next.definition.parameters.front().value = 1.f;
    EXPECT_FALSE(document.Apply("Bad default", std::move(next)));
    EXPECT_TRUE(std::holds_alternative<bool>(document.Source().definition.parameters.front().value));
}

//
// Created on 2026/11/10.
//

#include <curve/editor/CurveDocument.h>
#include <gtest/gtest.h>

namespace sky::curve::editor {
    namespace {
        struct Storage {
            std::string text   = R"({ "version":1, "channels":1, "interpolation":"linear", "preWrap":"clamp", "postWrap":"clamp",
                                  "keys":[{"time":0,"value":[0]},{"time":1,"value":[1]}] })";
            uint32_t    writes = 0, cooks = 0;
            bool        fail = false;

            CurveSourceStorage Bind()
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

    TEST(CurveDocumentState, UndoUsesCanonicalCheckpointAndSaveMovesIt)
    {
        Storage       storage;
        CurveDocument document(Uuid::Create(), storage.Bind());
        ASSERT_TRUE(document.Load());
        EXPECT_FALSE(document.IsDirty());
        auto next                = document.GetCurve();
        next.keys.back().value.x = 4.f;
        ASSERT_TRUE(document.Apply("Change value", next));
        EXPECT_TRUE(document.IsDirty());
        ASSERT_TRUE(document.Commands().Undo());
        EXPECT_FALSE(document.IsDirty());
        ASSERT_TRUE(document.Commands().Redo());
        EXPECT_TRUE(document.IsDirty());
        ASSERT_TRUE(document.Save());
        EXPECT_FALSE(document.IsDirty());
        ASSERT_TRUE(document.Commands().Undo());
        EXPECT_TRUE(document.IsDirty());
        ASSERT_TRUE(document.Commands().Redo());
        EXPECT_FALSE(document.IsDirty());
    }

    TEST(CurveDocumentState, SuccessfulReloadClearsHistoryAndPendingSave)
    {
        Storage       storage;
        CurveDocument document(Uuid::Create(), storage.Bind());
        ASSERT_TRUE(document.Load());
        auto next                = document.GetCurve();
        next.keys.back().value.x = 4.f;
        ASSERT_TRUE(document.Apply("Change value", next));
        document.RequestSave();
        ASSERT_TRUE(document.Load());
        EXPECT_FLOAT_EQ(document.GetCurve().keys.back().value.x, 1.f);
        EXPECT_FALSE(document.Commands().CanUndo());
        EXPECT_FALSE(document.Commands().CanRedo());
        EXPECT_FALSE(document.IsDirty());
        EXPECT_FALSE(document.IsSavePending());
        EXPECT_EQ(storage.writes, 0u);
    }

    TEST(CurveDocumentState, FailedReloadPreservesDraftHistoryAndCheckpoint)
    {
        Storage       storage;
        CurveDocument document(Uuid::Create(), storage.Bind());
        ASSERT_TRUE(document.Load());
        auto next                = document.GetCurve();
        next.keys.back().value.x = 4.f;
        ASSERT_TRUE(document.Apply("Change value", next));
        const auto revision = document.Revision();
        storage.text        = "invalid source";
        EXPECT_FALSE(document.Load());
        EXPECT_EQ(document.Revision(), revision);
        EXPECT_TRUE(document.IsLoaded());
        EXPECT_TRUE(document.IsDirty());
        EXPECT_FLOAT_EQ(document.GetCurve().keys.back().value.x, 4.f);
        ASSERT_TRUE(document.Commands().Undo());
        EXPECT_FALSE(document.IsDirty());
    }

    TEST(CurveDocumentState, SaveFailurePreservesWorkAndRetryClearsError)
    {
        Storage       storage;
        CurveDocument document(Uuid::Create(), storage.Bind());
        EXPECT_FALSE(document.Save());
        EXPECT_EQ(storage.writes, 0u);
        ASSERT_TRUE(document.Load());
        auto next                = document.GetCurve();
        next.keys.back().value.x = 4.f;
        ASSERT_TRUE(document.Apply("Change value", next));
        storage.fail        = true;
        const auto original = storage.text;
        document.RequestSave();
        EXPECT_FALSE(document.FlushPendingSave());
        EXPECT_FALSE(document.IsSavePending());
        EXPECT_TRUE(document.IsDirty());
        EXPECT_FALSE(document.LastError().empty());
        EXPECT_EQ(storage.text, original);
        EXPECT_EQ(storage.cooks, 0u);
        EXPECT_TRUE(document.Commands().CanUndo());
        storage.fail = false;
        document.RequestSave();
        document.RequestSave();
        ASSERT_TRUE(document.FlushPendingSave());
        ASSERT_TRUE(document.FlushPendingSave());
        EXPECT_FALSE(document.IsDirty());
        EXPECT_TRUE(document.LastError().empty());
        EXPECT_EQ(storage.writes, 2u);
        EXPECT_EQ(storage.cooks, 1u);
    }

    TEST(CurveDocumentState, NoOpEditPreservesHistoryAndRevision)
    {
        Storage       storage;
        CurveDocument document(Uuid::Create(), storage.Bind());
        ASSERT_TRUE(document.Load());
        const auto revision = document.Revision();
        ASSERT_TRUE(document.Apply("Unchanged", document.GetCurve()));
        EXPECT_EQ(document.Revision(), revision);
        EXPECT_FALSE(document.Commands().CanUndo());
        EXPECT_FALSE(document.IsDirty());
    }
} // namespace sky::curve::editor

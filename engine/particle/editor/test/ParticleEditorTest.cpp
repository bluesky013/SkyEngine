#include <editor/ui/AssetViewProvider.h>
#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>
#include <particle/editor/ParticleEditor.h>

using namespace sky;
using namespace sky::particle::editor;

namespace {
    struct Storage {
        std::string           text   = R"({"version":1,"emitters":[{"name":"sparks","simTarget":"CPU","capacity":16,"modules":[],"renderers":[]}]})";
        uint32_t              writes = 0, cooks = 0;
        bool                  fail = false;
        ParticleSourceStorage Bind()
        {
            return {[this](std::string &out) -> FileOperationStatus {
                        out = text;
                        return {};
                    },
                    [this](std::string_view value) -> FileOperationStatus {
                        ++writes;
                        if (fail) {
                            return {FileOperationError::IOError, "Injected write failure"};
                        }
                        text = value;
                        return {};
                    },
                    [this](const Uuid &) { ++cooks; }};
        }
    };
} // namespace

TEST(ParticleEditorDocument, LoadsSavesAndMarksDirty)
{
    Storage          storage;
    ParticleDocument document(Uuid::Create(), storage.Bind());
    ASSERT_TRUE(document.Load());
    EXPECT_FALSE(document.IsDirty());
    ASSERT_EQ(document.Effect().emitters.size(), 1u);
    document.Effect().emitters.front().capacity = 32;
    document.MarkEdited();
    EXPECT_TRUE(document.IsDirty());
    document.RequestSave();
    ASSERT_TRUE(document.FlushPendingSave());
    EXPECT_FALSE(document.IsDirty());
    EXPECT_EQ(storage.writes, 1u);
    EXPECT_EQ(storage.cooks, 1u);
}

TEST(ParticleEditorDocument, FailedSavePreservesDirtyWork)
{
    Storage          storage;
    ParticleDocument document(Uuid::Create(), storage.Bind());
    ASSERT_TRUE(document.Load());
    document.Effect().emitters.front().capacity = 64;
    document.MarkEdited();
    storage.fail = true;
    document.RequestSave();
    EXPECT_FALSE(document.FlushPendingSave());
    EXPECT_TRUE(document.IsDirty());
    EXPECT_FALSE(document.LastError().empty());
}

TEST(ParticleEditorTypes, ModuleParametersAreEditable)
{
    const auto *node = sky::GetTypeNode(TypeInfo<sky::particle::ParticleModuleDesc>::RegisteredId());
    ASSERT_NE(node, nullptr);
    EXPECT_NE(node->members.find("parameters"), node->members.end());
    const auto *parameter = sky::GetTypeNode(TypeInfo<sky::particle::ParticleParameterDesc>::RegisteredId());
    ASSERT_NE(parameter, nullptr);
    EXPECT_NE(parameter->members.find("name"), parameter->members.end());
    EXPECT_NE(parameter->members.find("value"), parameter->members.end());
}

TEST(ParticleEditorProvider, RegistersParticleEffectType)
{
    auto provider = CreateParticleViewProvider();
    ASSERT_TRUE(provider);
    EXPECT_TRUE(provider->UsesDocumentWindow());
    sky::editor::AssetViewProviderRegistry::Get()->Register("ParticleEffect", provider.get());
    EXPECT_EQ(sky::editor::AssetViewProviderRegistry::Get()->Find("ParticleEffect"), provider.get());
    sky::editor::AssetViewProviderRegistry::Get()->Unregister("ParticleEffect", provider.get());
}

TEST(ParticleEditorDocument, StructureEditsAndPropertiesShareStableUndoHistory)
{
    Storage          storage;
    ParticleDocument document(Uuid::Create(), storage.Bind());
    ASSERT_TRUE(document.Load());
    auto next                      = document.Effect();
    next.emitters.front().capacity = 32;
    ASSERT_TRUE(document.Apply("Change capacity", next));
    next.emitters.push_back(next.emitters.front());
    ASSERT_TRUE(document.Apply("Add emitter", next));
    ASSERT_EQ(document.Commands().GetUndoCount(), 2u);
    ASSERT_TRUE(document.Commands().Undo());
    EXPECT_EQ(document.Effect().emitters.size(), 1u);
    EXPECT_EQ(document.Effect().emitters.front().capacity, 32u);
    ASSERT_TRUE(document.Commands().Undo());
    EXPECT_EQ(document.Effect().emitters.front().capacity, 16u);
    EXPECT_FALSE(document.IsDirty());
    ASSERT_TRUE(document.Commands().Redo());
    ASSERT_TRUE(document.Commands().Redo());
    EXPECT_EQ(document.Effect().emitters.size(), 2u);
    EXPECT_EQ(document.Effect().emitters.front().capacity, 32u);
}

TEST(ParticleEditorDocument, InvalidDraftDoesNotReplaceSourceOnSave)
{
    Storage          storage;
    ParticleDocument document(Uuid::Create(), storage.Bind());
    ASSERT_TRUE(document.Load());
    auto next                      = document.Effect();
    next.emitters.front().capacity = 0;
    ASSERT_TRUE(document.Apply("Invalid capacity", std::move(next)));
    const auto before = storage.text;
    EXPECT_FALSE(document.Save());
    EXPECT_EQ(storage.writes, 0u);
    EXPECT_EQ(storage.text, before);
    EXPECT_TRUE(document.IsDirty());
    ASSERT_TRUE(document.Commands().Undo());
    EXPECT_FALSE(document.IsDirty());
    EXPECT_TRUE(document.Compiled().succeeded);
}

TEST(ParticleEditorDocument, NoOpPreservesHistoryAndRevision)
{
    Storage          storage;
    ParticleDocument document(Uuid::Create(), storage.Bind());
    ASSERT_TRUE(document.Load());
    const auto revision = document.Revision();
    ASSERT_TRUE(document.Apply("Unchanged", document.Effect()));
    EXPECT_EQ(document.Revision(), revision);
    EXPECT_FALSE(document.Commands().CanUndo());
    EXPECT_FALSE(document.IsDirty());
}

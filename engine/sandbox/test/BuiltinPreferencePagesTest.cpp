#include <core/archive/StreamArchive.h>
#include <editor/core/preferences/BuiltinPreferencePages.h>
#include <editor/core/preferences/FilePreferenceStorage.h>
#include <editor/core/preferences/PreferencesPersistence.h>
#include <editor/core/property/PropertyEditor.h>
#include <framework/serialization/JsonArchive.h>
#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>

#include <limits>
#include <sstream>

namespace sky::editor::builtin_preference_pages_test {
    namespace {
        class BuiltinPreferencePagesTest : public testing::Test {
        protected:
            void SetUp() override
            {
                pages = PreferencePageRegistry::Get();
                pages->Clear();
                RegisterEditorSettings(SerializationContext::Get());
            }
            void TearDown() override
            {
                pages->Clear();
            }
            PreferencePageRegistry *pages = nullptr;
        };
    } // namespace

    TEST_F(BuiltinPreferencePagesTest, BindingAndCleanupPreservePluginPagesRegisteredBeforeAndAfterTheHost)
    {
        GeneralSettings    general;
        EditorViewSettings editor;
        RenderingSettings  rendering;
        pages->Add({"plugin.before", "Plugin", &general, GetTypeNode(TypeInfo<GeneralSettings>::RegisteredId())});
        const auto before = pages->GetPages().front().registration;
        {
            BuiltinPreferencePages host(*pages);
            ASSERT_TRUE(host.Bind(general, editor, rendering).Succeeded());
            ASSERT_EQ(pages->GetPages().size(), 4u);
            EXPECT_EQ(pages->GetPages().front().registration, before);
            pages->Add({"plugin.after", "Plugin", &rendering, GetTypeNode(TypeInfo<RenderingSettings>::RegisteredId())});
            ASSERT_TRUE(host.Bind(general, editor, rendering).Succeeded());
            ASSERT_EQ(pages->GetPages().size(), 5u);
            EXPECT_EQ(pages->GetPages()[0].id, "plugin.before");
            EXPECT_EQ(pages->GetPages()[1].id, "plugin.after");
        }
        ASSERT_EQ(pages->GetPages().size(), 2u);
        EXPECT_EQ(pages->GetPages()[0].id, "plugin.before");
        EXPECT_EQ(pages->GetPages()[1].id, "plugin.after");
    }

    TEST_F(BuiltinPreferencePagesTest, CleanupDoesNotRemoveAReplacementWithTheSamePageId)
    {
        GeneralSettings        general;
        EditorViewSettings     editor;
        RenderingSettings      rendering;
        BuiltinPreferencePages host(*pages);
        ASSERT_TRUE(host.Bind(general, editor, rendering).Succeeded());
        ASSERT_TRUE(pages->Remove("general"));
        pages->Add({"general", "Replacement", &general, GetTypeNode(TypeInfo<GeneralSettings>::RegisteredId())});
        const auto replacement = pages->GetPages().back().registration;
        const auto collision   = host.Bind(general, editor, rendering);
        EXPECT_EQ(collision.code, PreferenceCode::Conflict);
        EXPECT_EQ(pages->GetPages().size(), 3u);
        host.Reset();
        ASSERT_EQ(pages->GetPages().size(), 1u);
        EXPECT_EQ(pages->GetPages().front().registration, replacement);
        host.Reset();
        EXPECT_EQ(pages->GetPages().size(), 1u);
    }

    TEST_F(BuiltinPreferencePagesTest, ExistingBuiltinIdRejectsTheEntireBindingWithoutErasingTheContributor)
    {
        GeneralSettings    general;
        EditorViewSettings editor;
        RenderingSettings  rendering;
        pages->Add({"editor", "Plugin collision", &editor, GetTypeNode(TypeInfo<EditorViewSettings>::RegisteredId())});
        const auto             original = pages->GetPages().front().registration;
        BuiltinPreferencePages host(*pages);
        const auto             result = host.Bind(general, editor, rendering);
        EXPECT_EQ(result.code, PreferenceCode::Conflict);
        EXPECT_EQ(result.pageId, "editor");
        EXPECT_FALSE(result.HasPartialEffects());
        ASSERT_EQ(pages->GetPages().size(), 1u);
        EXPECT_EQ(pages->GetPages().front().registration, original);
    }

    TEST_F(BuiltinPreferencePagesTest, AllBuiltinValidationRunsBeforeAnyLiveValueOrApplicationCallback)
    {
        GeneralSettings        general;
        EditorViewSettings     editor;
        RenderingSettings      rendering;
        int                    applications = 0;
        BuiltinPreferencePages host(*pages);
        ASSERT_TRUE(host.Bind(general, editor, rendering,
                              [&] {
                                  ++applications;
                                  return PreferenceResult{};
                              })
                        .Succeeded());
        std::vector<PreferenceValue> values;
        ASSERT_TRUE(CapturePreferenceValues(pages->GetPages(), values).Succeeded());
        static_cast<EditorViewSettings *>(values[1].value.Data())->uiScale = 1.5f;
        auto *candidate                                                    = static_cast<GeneralSettings *>(values[0].value.Data());
        candidate->language                                                = static_cast<EditorLanguage>(9);
        EXPECT_EQ(ValidatePreferenceValues(*pages, values).code, PreferenceCode::ValidationFailed);
        candidate->language = EditorLanguage::English;
        candidate->theme    = static_cast<EditorTheme>(-1);
        EXPECT_EQ(ValidatePreferenceValues(*pages, values).path, "theme");
        candidate->theme = EditorTheme::Dark;
        auto *view       = static_cast<EditorViewSettings *>(values[1].value.Data());
        view->uiScale    = std::numeric_limits<float>::quiet_NaN();
        EXPECT_EQ(ValidatePreferenceValues(*pages, values).path, "uiScale");
        view->uiScale   = 1.5f;
        view->undoDepth = 0;
        EXPECT_EQ(ValidatePreferenceValues(*pages, values).path, "undoDepth");
        view->undoDepth         = 32;
        auto *render            = static_cast<RenderingSettings *>(values[2].value.Data());
        render->resolutionScale = 3.0f;
        EXPECT_EQ(ValidatePreferenceValues(*pages, values).path, "resolutionScale");
        EXPECT_FLOAT_EQ(editor.uiScale, 1.0f);
        EXPECT_EQ(applications, 0);
        render->resolutionScale = 1.0f;
        ASSERT_TRUE(ValidatePreferenceValues(*pages, values).Succeeded());
        ASSERT_TRUE(PublishPreferenceValues(*pages, values).Succeeded());
        EXPECT_FLOAT_EQ(editor.uiScale, 1.5f);
        EXPECT_EQ(applications, 1);
    }

    TEST_F(BuiltinPreferencePagesTest, StoredOnlySettingsRemainReflectedAndTheirApplicationStatusIsVisible)
    {
        GeneralSettings        general;
        EditorViewSettings     editor;
        RenderingSettings      rendering;
        BuiltinPreferencePages host(*pages);
        ASSERT_TRUE(host.Bind(general, editor, rendering).Succeeded());
        EXPECT_NE(pages->GetPages()[0].title.find("stored only"), std::string::npos);
        EXPECT_NE(pages->GetPages()[2].title.find("stored only"), std::string::npos);
        const auto *node = GetTypeNode(TypeInfo<EditorViewSettings>::RegisteredId());
        ASSERT_NE(node, nullptr);
        EXPECT_NE(ReadPropertyAttributes(node->members.at("gridSize")).label.find("stored only"), std::string::npos);
        EXPECT_NE(ReadPropertyAttributes(node->members.at("uiScale")).tooltip.find("immediately"), std::string::npos);
        EXPECT_FALSE(ReadPropertyAttributes(node->members.at("gridSize")).readOnly);
    }

    TEST_F(BuiltinPreferencePagesTest, ScaleCompositionClampsEachInputAndTheProductWithoutPropagatingNan)
    {
        EXPECT_FLOAT_EQ(ComputeEditorUiScale(1.5f, 1.5f), 2.25f);
        EXPECT_FLOAT_EQ(ComputeEditorUiScale(2.0f, 1.5f), 3.0f);
        EXPECT_FLOAT_EQ(ComputeEditorUiScale(4.0f, 2.0f), 4.0f);
        EXPECT_FLOAT_EQ(ComputeEditorUiScale(0.5f, 0.5f), 0.5f);
        EXPECT_FLOAT_EQ(ComputeEditorUiScale(std::numeric_limits<float>::quiet_NaN(), 1.5f), 1.5f);
        EXPECT_FLOAT_EQ(ComputeEditorUiScale(2.0f, std::numeric_limits<float>::infinity()), 2.0f);
    }

    TEST_F(BuiltinPreferencePagesTest, LegacyThreePageArchiveLoadsThroughTheCheckedProtocolAndPreservesStoredOnlyValues)
    {
        GeneralSettings originalGeneral;
        originalGeneral.language = EditorLanguage::Chinese;
        originalGeneral.theme    = EditorTheme::Light;
        originalGeneral.autosave = false;
        EditorViewSettings originalEditor;
        originalEditor.uiScale   = 1.5f;
        originalEditor.gridSize  = 25.0f;
        originalEditor.snap      = false;
        originalEditor.undoDepth = 64;
        RenderingSettings originalRendering;
        originalRendering.vsync           = false;
        originalRendering.resolutionScale = 0.75f;
        std::ostringstream bytes;
        {
            OStreamArchive    stream(bytes);
            JsonOutputArchive json(stream);
            json.StartObject();
            json.Key("general");
            json.SaveValueObject(&originalGeneral, TypeInfo<GeneralSettings>::RegisteredId());
            json.Key("editor");
            json.SaveValueObject(&originalEditor, TypeInfo<EditorViewSettings>::RegisteredId());
            json.Key("rendering");
            json.SaveValueObject(&originalRendering, TypeInfo<RenderingSettings>::RegisteredId());
            json.EndObject();
        }
        class LegacyStorage final : public IPreferenceStorage {
        public:
            std::string      bytes;
            PreferenceResult Read(std::string &out) override
            {
                out = bytes;
                return {};
            }
            PreferenceResult Write(const std::string &) override
            {
                return {PreferenceCode::WriteFailed};
            }
        };
        auto storage   = std::make_shared<LegacyStorage>();
        storage->bytes = bytes.str();
        GeneralSettings        general;
        EditorViewSettings     editor;
        RenderingSettings      rendering;
        int                    applications = 0;
        BuiltinPreferencePages host(*pages);
        ASSERT_TRUE(host.Bind(general, editor, rendering,
                              [&] {
                                  ++applications;
                                  return PreferenceResult{};
                              })
                        .Succeeded());
        PreferencesPersistence persistence(storage);
        const auto             loaded = persistence.Load(*pages);
        ASSERT_TRUE(loaded.Succeeded()) << loaded.message;
        EXPECT_FALSE(loaded.storageWritten);
        EXPECT_EQ(general.language, EditorLanguage::Chinese);
        EXPECT_EQ(general.theme, EditorTheme::Light);
        EXPECT_FALSE(general.autosave);
        EXPECT_FLOAT_EQ(editor.uiScale, 1.5f);
        EXPECT_FLOAT_EQ(editor.gridSize, 25.0f);
        EXPECT_FALSE(editor.snap);
        EXPECT_EQ(editor.undoDepth, 64);
        EXPECT_FALSE(rendering.vsync);
        EXPECT_FLOAT_EQ(rendering.resolutionScale, 0.75f);
        EXPECT_EQ(applications, 1);
    }
} // namespace sky::editor::builtin_preference_pages_test

#include <core/environment/Once.h>
#include <editor/core/EditorCore.h>
#include <editor/core/layout/LayoutModel.h>
#include <editor/core/layout/PanelRegistry.h>
#include <editor/core/preferences/PreferencesPersistence.h>
#include <editor/shell/EditorShell.h>
#include <editor/shell/PreferencesDialog.h>
#include <editor/ui/UiTheme.h>
#include <editor/widgets/ReflectedFormView.h>
#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace sky::editor::preferences_persistence_shell_test {

    struct Settings {
        int32_t value   = 1;
        float   uiScale = 1.0f;
    };

    namespace {
        class MemoryStorage : public IPreferenceStorage {
        public:
            PreferenceResult Read(std::string &document) override
            {
                document = bytes;
                return {};
            }
            PreferenceResult Write(const std::string &document) override
            {
                ++writes;
                if (failWrite)
                    return {PreferenceCode::WriteFailed, {}, {}, "injected write failure"};
                bytes               = document;
                const auto callback = onWrite;
                if (callback)
                    callback();
                return {};
            }
            bool                  failWrite = false;
            int                   writes    = 0;
            std::string           bytes     = "{}";
            std::function<void()> onWrite;
        };

        template <class T>
        T *Find(ui::UIElement &element)
        {
            if (auto *found = dynamic_cast<T *>(&element))
                return found;
            for (const auto &child : element.GetChildren()) {
                if (auto *found = Find<T>(*child))
                    return found;
            }
            return nullptr;
        }

        PropertyField *Field(ReflectedFormView &form, const std::string &path)
        {
            for (auto &section : form.Form().GetSections()) {
                for (auto &field : section.fields) {
                    if (field.path == path)
                        return &field;
                }
            }
            return nullptr;
        }

        class PreferencesPersistenceShellTest : public testing::Test {
        protected:
            void SetUp() override
            {
                theme = GetDefaultUiTheme();
                SetDefaultUiTheme(MakeDarkTheme());
                if (Once::Get()->Run("PreferencesPersistenceShellTest.Settings")) {
                    SerializationContext::Get()
                        ->Register<Settings>("PreferencesPersistenceShellSettings")
                        .Member<&Settings::value>("value")
                        .Member<&Settings::uiScale>("uiScale");
                }
                pages = PreferencePageRegistry::Get();
                pages->Clear();
                EditorCore::GetCommandService().Clear();
                storage     = std::make_shared<MemoryStorage>();
                persistence = std::make_shared<PreferencesPersistence>(storage);
            }
            void TearDown() override
            {
                pages->Clear();
                EditorCore::GetCommandService().Clear();
                SetDefaultUiTheme(theme);
            }
            PreferencePage Page(Settings &live)
            {
                return {"portable", "Portable", &live, GetTypeNode(TypeInfo<Settings>::RegisteredId())};
            }
            void Open(PreferencesDialog &dialog)
            {
                dialog.SetModel(pages);
                dialog.SetPersistence(persistence);
                dialog.Open();
                dialog.ArrangeIfNeeded({0.0f, 0.0f, 1000.0f, 760.0f});
            }
            template <class T>
            static void Edit(PreferencesDialog &dialog, const std::string &path, T value)
            {
                auto *form = Find<ReflectedFormView>(dialog);
                ASSERT_NE(form, nullptr);
                auto *field = Field(*form, path);
                ASSERT_NE(field, nullptr);
                ASSERT_TRUE(form->Form().Edit(*field, Any(value), dialog.History()));
            }
            static int32_t Working(PreferencesDialog &dialog)
            {
                auto *form = Find<ReflectedFormView>(dialog);
                EXPECT_NE(form, nullptr);
                auto *field = form ? Field(*form, "value") : nullptr;
                EXPECT_NE(field, nullptr);
                const Any   value  = field ? field->descriptor.GetValue() : Any{};
                const auto *number = value.GetAsConst<int32_t>();
                EXPECT_NE(number, nullptr);
                return number ? *number : 0;
            }
            static void Click(PreferencesDialog &dialog, int button)
            {
                const auto         rect = dialog.ButtonRect(button);
                ui::UIPointerEvent event;
                event.action = ui::UIPointerAction::DOWN;
                event.x      = (rect.left + rect.right) * 0.5f;
                event.y      = (rect.top + rect.bottom) * 0.5f;
                dialog.OnPointerEvent(event);
            }
            PreferencePageRegistry                 *pages = nullptr;
            std::shared_ptr<MemoryStorage>          storage;
            std::shared_ptr<PreferencesPersistence> persistence;
            UiTheme                                 theme;
        };
    } // namespace

    TEST_F(PreferencesPersistenceShellTest, FailedOkKeepsTheDialogAndHistoryUntilACompleteRetryClosesIt)
    {
        Settings live;
        pages->Add(Page(live));
        int                           applied = 0;
        int                           closed  = 0;
        std::vector<PreferenceResult> results;
        PreferencesDialog             dialog(nullptr);
        dialog.SetOnApplied([&]() { ++applied; });
        dialog.SetOnClosed([&]() { ++closed; });
        dialog.SetOnApplyResult([&](const PreferenceResult &result) { results.push_back(result); });
        Open(dialog);
        Edit(dialog, "value", int32_t{7});
        storage->failWrite = true;
        Click(dialog, 0);
        EXPECT_TRUE(dialog.IsOpen());
        EXPECT_EQ(dialog.GetLastApplyResult().code, PreferenceCode::WriteFailed);
        EXPECT_FALSE(dialog.GetLastApplyResult().HasPartialEffects());
        EXPECT_EQ(live.value, 1);
        EXPECT_EQ(Working(dialog), 7);
        EXPECT_EQ(dialog.History().GetUndoCount(), 1u);
        EXPECT_EQ(applied, 0);
        EXPECT_EQ(closed, 0);
        ASSERT_EQ(results.size(), 1u);
        EXPECT_EQ(results.front().code, PreferenceCode::WriteFailed);
        EXPECT_EQ(storage->bytes, "{}");
        storage->failWrite = false;
        Click(dialog, 0);
        EXPECT_FALSE(dialog.IsOpen());
        EXPECT_TRUE(dialog.GetLastApplyResult().Succeeded());
        EXPECT_TRUE(dialog.GetLastApplyResult().storageWritten);
        EXPECT_EQ(live.value, 7);
        EXPECT_FALSE(dialog.History().CanUndo());
        EXPECT_EQ(applied, 1);
        EXPECT_EQ(closed, 1);
        ASSERT_EQ(results.size(), 2u);
        EXPECT_TRUE(results.back().Succeeded());
    }

    TEST_F(PreferencesPersistenceShellTest, RejectedApplicationReportsPartialEffectsAndCancelDoesNotRollThemBack)
    {
        Settings live;
        auto     page = Page(live);
        page.apply    = [](const PreferencePage &) { return PreferenceResult{PreferenceCode::ApplyFailed, "portable", {}, "application rejected"}; };
        pages->Add(std::move(page));
        int               applied = 0;
        int               closed  = 0;
        PreferencesDialog dialog(nullptr);
        dialog.SetOnApplied([&]() { ++applied; });
        dialog.SetOnClosed([&]() { ++closed; });
        Open(dialog);
        Edit(dialog, "value", int32_t{7});
        dialog.Apply();
        const auto result = dialog.GetLastApplyResult();
        EXPECT_EQ(result.code, PreferenceCode::ApplyFailed);
        EXPECT_TRUE(result.storageWritten);
        EXPECT_TRUE(result.liveWritten);
        EXPECT_TRUE(result.applicationApplied);
        EXPECT_TRUE(result.HasPartialEffects());
        EXPECT_TRUE(dialog.IsOpen());
        EXPECT_EQ(dialog.History().GetUndoCount(), 1u);
        EXPECT_EQ(applied, 0);
        EXPECT_EQ(live.value, 7);
        const auto persisted = storage->bytes;
        EXPECT_NE(persisted, "{}");
        Click(dialog, 1);
        EXPECT_FALSE(dialog.IsOpen());
        EXPECT_EQ(closed, 1);
        EXPECT_EQ(live.value, 7);
        EXPECT_EQ(storage->bytes, persisted);
        EXPECT_FALSE(dialog.History().CanUndo());
    }

    TEST_F(PreferencesPersistenceShellTest, ShellPersistenceGatesThePortableThemeApplicationHook)
    {
        Settings    live;
        LayoutModel layout;
        layout.SetDefault({"main"});
        PanelRegistry panels;
        panels.Register(PanelInfo{"main", "Main", 0.0f, 0.0f, nullptr});
        EditorShell shell;
        shell.SetUiScale(1.0f);
        shell.SetLayout(&layout);
        shell.SetPanelRegistry(&panels);
        shell.Rebuild();
        shell.Layout(1000.0f, 760.0f);
        int  applications  = 0;
        int  notifications = 0;
        auto page          = Page(live);
        page.apply         = [&shell, &applications](const PreferencePage &applied) {
            ++applications;
            shell.SetUiScale(static_cast<const Settings *>(applied.object)->uiScale);
            return PreferenceResult{};
        };
        pages->Add(std::move(page));
        shell.SetPreferences(pages, [&]() { ++notifications; });
        shell.SetPreferencesPersistence(persistence);
        shell.OpenPreferences();
        auto *dialog = dynamic_cast<PreferencesDialog *>(shell.HitTest(1.0f, 1.0f));
        ASSERT_NE(dialog, nullptr);
        Edit(*dialog, "uiScale", 1.5f);
        const auto revision = GetDefaultUiThemeRevision();
        storage->failWrite  = true;
        dialog->Apply();
        EXPECT_EQ(dialog->GetLastApplyResult().code, PreferenceCode::WriteFailed);
        EXPECT_EQ(applications, 0);
        EXPECT_EQ(notifications, 0);
        EXPECT_FLOAT_EQ(live.uiScale, 1.0f);
        EXPECT_FLOAT_EQ(GetDefaultUiTheme().scale, 1.0f);
        EXPECT_EQ(GetDefaultUiThemeRevision(), revision);
        storage->failWrite = false;
        dialog->Apply();
        EXPECT_TRUE(dialog->GetLastApplyResult().Succeeded());
        EXPECT_EQ(applications, 1);
        EXPECT_EQ(notifications, 1);
        EXPECT_FLOAT_EQ(live.uiScale, 1.5f);
        EXPECT_FLOAT_EQ(GetDefaultUiTheme().scale, 1.5f);
        EXPECT_GT(GetDefaultUiThemeRevision(), revision);
        EXPECT_FALSE(dialog->History().CanUndo());
    }

    TEST_F(PreferencesPersistenceShellTest, ResultAndStorageCallbacksCanReplaceSessionsPipelinesOrDeleteTheDialog)
    {
        Settings live;
        pages->Add(Page(live));
        int  staleApplied = 0;
        int  staleClosed  = 0;
        auto dialog       = std::make_unique<PreferencesDialog>(nullptr);
        Open(*dialog);
        Edit(*dialog, "value", int32_t{7});
        dialog->SetOnApplied([&]() { ++staleApplied; });
        dialog->SetOnClosed([&]() { ++staleClosed; });
        dialog->SetOnApplyResult([&](const PreferenceResult &result) {
            if (result.Succeeded()) {
                dialog->Open();
                Edit(*dialog, "value", int32_t{99});
            }
        });
        Click(*dialog, 0);
        ASSERT_NE(dialog, nullptr);
        EXPECT_TRUE(dialog->IsOpen());
        EXPECT_EQ(Working(*dialog), 99);
        EXPECT_EQ(dialog->History().GetUndoCount(), 1u);
        EXPECT_EQ(staleApplied, 0);
        EXPECT_EQ(staleClosed, 0);
        EXPECT_EQ(live.value, 7);

        storage->failWrite = true;
        dialog->SetOnApplyResult([&](const PreferenceResult &) { dialog->Close(); });
        dialog->Apply();
        EXPECT_FALSE(dialog->IsOpen());
        EXPECT_EQ(live.value, 7);
        dialog->Open();
        Edit(*dialog, "value", int32_t{9});
        dialog->SetOnApplyResult([&](const PreferenceResult &) { dialog.reset(); });
        dialog->Apply();
        EXPECT_EQ(dialog, nullptr);
        EXPECT_EQ(live.value, 7);

        storage->failWrite      = false;
        auto replacementStorage = std::make_shared<MemoryStorage>();
        auto replacement        = std::make_shared<PreferencesPersistence>(replacementStorage);
        dialog                  = std::make_unique<PreferencesDialog>(nullptr);
        Open(*dialog);
        Edit(*dialog, "value", int32_t{9});
        storage->onWrite = [&]() { dialog->SetPersistence(replacement); };
        dialog->Apply();
        EXPECT_EQ(dialog->GetLastApplyResult().code, PreferenceCode::Conflict);
        EXPECT_TRUE(dialog->GetLastApplyResult().storageWritten);
        EXPECT_FALSE(dialog->GetLastApplyResult().liveWritten);
        EXPECT_EQ(live.value, 7);
        EXPECT_EQ(Working(*dialog), 9);
        EXPECT_EQ(dialog->History().GetUndoCount(), 1u);
        dialog->Apply();
        EXPECT_TRUE(dialog->GetLastApplyResult().Succeeded());
        EXPECT_EQ(replacementStorage->writes, 1);
        EXPECT_EQ(live.value, 9);
        Edit(*dialog, "value", int32_t{11});
        replacementStorage->onWrite = [&]() { dialog.reset(); };
        dialog->Apply();
        EXPECT_EQ(dialog, nullptr);
        EXPECT_EQ(live.value, 9);
    }

} // namespace sky::editor::preferences_persistence_shell_test

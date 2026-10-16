#include <core/environment/Once.h>
#include <core/util/Uuid.h>
#include <editor/core/EditorCore.h>
#include <editor/core/extension/EditorActionRegistry.h>
#include <editor/core/layout/LayoutModel.h>
#include <editor/core/layout/PanelRegistry.h>
#include <editor/core/preferences/BuiltinPreferencePages.h>
#include <editor/core/preferences/FilePreferenceStorage.h>
#include <editor/shell/EditorShell.h>
#include <editor/shell/PreferencesDialog.h>
#include <editor/ui/UiTheme.h>
#include <editor/widgets/ReflectedFormView.h>
#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>
#include <ui/UIElement.h>
#include <ui/UIPaintContext.h>

#include <filesystem>
#include <fstream>
#include <limits>

namespace sky::editor::editor_preference_integration_test {
    struct PluginSettings {
        int32_t     value = 3;
        std::string label = "插件 default";
    };
    namespace {
        class ScaleInputProbe : public ui::UIElement {
        public:
            ScaleInputProbe()
            {
                SetFocusable(true);
            }
            ui::UIEventResult OnPointerEvent(const ui::UIPointerEvent &) override
            {
                pointerScale = GetDefaultUiTheme().scale;
                return ui::UIEventResult::HANDLED;
            }
            ui::UIEventResult OnKeyEvent(const ui::UIKeyEvent &) override
            {
                keyScale = GetDefaultUiTheme().scale;
                return ui::UIEventResult::HANDLED;
            }
            ui::UIEventResult OnTextInput(const ui::UITextInputEvent &) override
            {
                textScale = GetDefaultUiTheme().scale;
                return ui::UIEventResult::HANDLED;
            }
            float pointerScale = 0.0f;
            float keyScale     = 0.0f;
            float textScale    = 0.0f;
        };
        template <class T>
        T *Find(ui::UIElement &element)
        {
            if (auto *found = dynamic_cast<T *>(&element))
                return found;
            for (const auto &child : element.GetChildren())
                if (auto *found = Find<T>(*child))
                    return found;
            return nullptr;
        }
        class EditorPreferenceIntegrationTest : public testing::Test {
        protected:
            void SetUp() override
            {
                theme = GetDefaultUiTheme();
                SetDefaultUiTheme(MakeDarkTheme());
                pages = PreferencePageRegistry::Get();
                pages->Clear();
                EditorActionRegistry::Get()->Clear();
                EditorCore::GetCommandService().Clear();
                if (Once::Get()->Run("EditorPreferenceIntegrationTest.Plugin")) {
                    SerializationContext::Get()
                        ->Register<PluginSettings>("EditorPreferenceIntegrationPlugin")
                        .Member<&PluginSettings::value>("value")
                        .Member<&PluginSettings::label>("label");
                }
                root = std::filesystem::temp_directory_path() / ("sky-editor-preferences-" + Uuid::Create().ToString());
                std::filesystem::create_directories(root);
            }
            void TearDown() override
            {
                pages->Clear();
                EditorActionRegistry::Get()->Clear();
                EditorCore::GetCommandService().Clear();
                SetDefaultUiTheme(theme);
                std::error_code ignored;
                std::filesystem::remove_all(root, ignored);
            }
            static void Configure(EditorShell &shell, LayoutModel &layout, PanelRegistry &panels)
            {
                layout.SetDefault({"main", "floating"});
                for (const auto *id : {"main", "floating"}) {
                    panels.Register(PanelInfo{id, id, 0.0f, 0.0f, nullptr});
                    shell.RegisterPanelView(id, [] { return std::make_unique<ui::UIElement>(); });
                }
                shell.SetLayout(&layout);
                shell.SetPanelRegistry(&panels);
                shell.Rebuild();
                shell.Layout(1000.0f, 760.0f);
            }
            static PreferencesDialog *Open(EditorShell &shell)
            {
                shell.OpenPreferences();
                auto *dialog = dynamic_cast<PreferencesDialog *>(shell.HitTest(1.0f, 1.0f));
                EXPECT_NE(dialog, nullptr);
                return dialog;
            }
            template <class T>
            static void Edit(PreferencesDialog &dialog, const std::string &path, T value)
            {
                dialog.SynchronizeLayout();
                auto *form = Find<ReflectedFormView>(dialog);
                ASSERT_NE(form, nullptr);
                for (auto &section : form->Form().GetSections()) {
                    for (auto &field : section.fields) {
                        if (field.path == path) {
                            ASSERT_TRUE(form->Form().Edit(field, Any(std::move(value)), dialog.History()));
                            return;
                        }
                    }
                }
                FAIL() << "Missing reflected field: " << path;
            }
            PreferencePageRegistry *pages = nullptr;
            std::filesystem::path   root;
            UiTheme                 theme;
        };
    } // namespace

    TEST_F(EditorPreferenceIntegrationTest, UserZoomComposesWithEachDeviceDpiForExistingAndFutureFloatingWindows)
    {
        LayoutModel   layout;
        PanelRegistry panels;
        EditorShell   shell;
        shell.SetUiScale(1.5f);
        Configure(shell, layout, panels);
        shell.SetUserUiScale(1.5f);
        EXPECT_FLOAT_EQ(shell.GetSurfaceUiScale(), 2.25f);
        EXPECT_FLOAT_EQ(shell.GetSurfaceUiScale(99), 2.25f);
        shell.SetSurfaceScale(7, 2.0f);
        ASSERT_EQ(shell.FloatPanelToSurface("floating", 7, FloatingPanel{}), 7u);
        EXPECT_FLOAT_EQ(shell.GetSurfaceUiScale(7), 3.0f);
        ui::UIPaintContext paint;
        shell.PaintSurface(7, paint, 640.0f, 480.0f);
        EXPECT_FLOAT_EQ(GetDefaultUiTheme().scale, 3.0f);
        shell.PaintSurface(0, paint, 1000.0f, 760.0f);
        EXPECT_FLOAT_EQ(GetDefaultUiTheme().scale, 2.25f);
        shell.SetUserUiScale(0.75f);
        EXPECT_FLOAT_EQ(shell.GetSurfaceUiScale(7), 1.5f);
        EXPECT_FLOAT_EQ(shell.GetSurfaceUiScale(), 1.125f);
        shell.SetSurfaceScale(7, 1.5f);
        EXPECT_FLOAT_EQ(shell.GetSurfaceUiScale(7), 1.125f);
        shell.SetSurfaceScale(0, 2.0f);
        EXPECT_FLOAT_EQ(shell.GetSurfaceUiScale(), 1.5f);
        shell.SetUserUiScale(std::numeric_limits<float>::quiet_NaN());
        EXPECT_FLOAT_EQ(shell.GetSurfaceUiScale(), 2.0f);
        shell.SetUserUiScale(8.0f);
        EXPECT_FLOAT_EQ(shell.GetSurfaceUiScale(), 4.0f);
    }

    TEST_F(EditorPreferenceIntegrationTest, RealFileAndShellInjectionPersistPluginAndBuiltinPagesAcrossFreshHostBindings)
    {
        PluginSettings plugin;
        pages->Add({"plugin", "Plugin", &plugin, GetTypeNode(TypeInfo<PluginSettings>::RegisteredId())});
        GeneralSettings    general;
        EditorViewSettings editor;
        RenderingSettings  rendering;
        LayoutModel        layout;
        PanelRegistry      panels;
        EditorShell        shell;
        shell.SetUiScale(1.5f);
        BuiltinPreferencePages host(*pages);
        ASSERT_TRUE(host.Bind(general, editor, rendering,
                              [&] {
                                  shell.SetUserUiScale(editor.uiScale);
                                  return PreferenceResult{};
                              })
                        .Succeeded());
        const auto path        = root / "editor-preferences.json";
        auto       persistence = std::make_shared<PreferencesPersistence>(std::make_shared<FilePreferenceStorage>(path));
        EXPECT_EQ(persistence->Load(*pages).code, PreferenceCode::NotFound);
        shell.SetPreferencesPersistence(persistence);
        shell.SetPreferences(pages, {});
        Configure(shell, layout, panels);
        auto *dialog = Open(shell);
        ASSERT_NE(dialog, nullptr);
        ASSERT_EQ(dialog->GetPageCount(), 4);
        Edit(*dialog, "value", int32_t{17});
        Edit(*dialog, "label", std::string("插件 saved"));
        dialog->SetPageIndex(2);
        Edit(*dialog, "uiScale", 1.5f);
        EXPECT_EQ(plugin.value, 3);
        EXPECT_FLOAT_EQ(shell.GetSurfaceUiScale(), 1.5f);
        dialog->Apply();
        ASSERT_TRUE(dialog->GetLastApplyResult().Succeeded()) << dialog->GetLastApplyResult().message;
        EXPECT_TRUE(dialog->GetLastApplyResult().storageWritten);
        EXPECT_EQ(plugin.value, 17);
        EXPECT_FLOAT_EQ(shell.GetSurfaceUiScale(), 2.25f);
        EXPECT_FALSE(dialog->History().CanUndo());
        dialog->Close();
        host.Reset();
        ASSERT_EQ(pages->GetPages().size(), 1u);
        plugin = {};
        GeneralSettings        freshGeneral;
        EditorViewSettings     freshEditor;
        RenderingSettings      freshRendering;
        BuiltinPreferencePages reopenedHost(*pages);
        ASSERT_TRUE(reopenedHost
                        .Bind(freshGeneral, freshEditor, freshRendering,
                              [&] {
                                  shell.SetUserUiScale(freshEditor.uiScale);
                                  return PreferenceResult{};
                              })
                        .Succeeded());
        PreferencesPersistence reopened(std::make_shared<FilePreferenceStorage>(path));
        const auto             loaded = reopened.Load(*pages);
        ASSERT_TRUE(loaded.Succeeded()) << loaded.message;
        EXPECT_EQ(plugin.value, 17);
        EXPECT_EQ(plugin.label, "插件 saved");
        EXPECT_FLOAT_EQ(freshEditor.uiScale, 1.5f);
        EXPECT_FLOAT_EQ(shell.GetSurfaceUiScale(), 2.25f);
        shell.SetPreferences(nullptr, {});
    }

    TEST_F(EditorPreferenceIntegrationTest, EachInputEntrySelectsItsWindowThemeAfterAnotherWindowPaints)
    {
        GeneralSettings        general;
        EditorViewSettings     editor;
        RenderingSettings      rendering;
        BuiltinPreferencePages host(*pages);
        ASSERT_TRUE(host.Bind(general, editor, rendering).Succeeded());
        LayoutModel layout;
        layout.SetDefault({"main", "floating"});
        PanelRegistry panels;
        panels.Register(PanelInfo{"main", "Main", 0.0f, 0.0f, nullptr});
        panels.Register(PanelInfo{"floating", "Floating", 0.0f, 0.0f, nullptr});
        EditorShell shell;
        shell.SetLayout(&layout);
        shell.SetPanelRegistry(&panels);
        shell.RegisterPanelView("main", [] { return std::make_unique<ui::UIElement>(); });
        ScaleInputProbe *floating = nullptr;
        shell.RegisterPanelView("floating", [&] {
            auto value = std::make_unique<ScaleInputProbe>();
            floating   = value.get();
            return value;
        });
        shell.Rebuild();
        shell.Layout(1000.0f, 760.0f);
        shell.SetPreferences(pages, {});
        shell.SetSurfaceScale(7, 2.0f);
        ASSERT_EQ(shell.FloatPanelToSurface("floating", 7, FloatingPanel{}), 7u);
        ASSERT_NE(floating, nullptr);
        auto *dialog = Open(shell);
        ASSERT_NE(dialog, nullptr);
        const auto         cancel = dialog->ButtonRect(1);
        ui::UIPaintContext paint;
        shell.PaintSurface(7, paint, 640.0f, 480.0f);
        EXPECT_FLOAT_EQ(GetDefaultUiTheme().scale, 2.0f);
        ui::UIPointerEvent pointer;
        pointer.action = ui::UIPointerAction::DOWN;
        pointer.x      = (cancel.left + cancel.right) * 0.5f;
        pointer.y      = (cancel.top + cancel.bottom) * 0.5f;
        EXPECT_TRUE(shell.DispatchPointer(pointer));
        EXPECT_FALSE(dialog->IsOpen());
        EXPECT_FLOAT_EQ(GetDefaultUiTheme().scale, 1.0f);
        pointer.x = 100.0f;
        pointer.y = 100.0f;
        EXPECT_TRUE(shell.DispatchPointerToSurface(7, pointer));
        EXPECT_FLOAT_EQ(floating->pointerScale, 2.0f);
        shell.PaintSurface(0, paint, 1000.0f, 760.0f);
        ui::UIKeyEvent key;
        key.action  = ui::UIKeyAction::DOWN;
        key.keyCode = 'A';
        EXPECT_TRUE(shell.DispatchKeyToSurface(7, key));
        EXPECT_FLOAT_EQ(floating->keyScale, 2.0f);
        shell.PaintSurface(0, paint, 1000.0f, 760.0f);
        ui::UITextInputEvent text;
        text.text = "x";
        EXPECT_TRUE(shell.DispatchTextToSurface(7, text));
        EXPECT_FLOAT_EQ(floating->textScale, 2.0f);
        shell.DispatchKey(key);
        EXPECT_FLOAT_EQ(GetDefaultUiTheme().scale, 1.0f);
        shell.DispatchText(text);
        EXPECT_FLOAT_EQ(GetDefaultUiTheme().scale, 1.0f);
    }

    TEST_F(EditorPreferenceIntegrationTest, ReopeningPreferencesCanDestroyShellFromHistoryNotifications)
    {
        PluginSettings live;
        pages->Add({"plugin", "Plugin", &live, GetTypeNode(TypeInfo<PluginSettings>::RegisteredId())});
        for (const int deleteAt : {1, 3}) {
            LayoutModel   layout;
            PanelRegistry panels;
            auto          shell = std::make_unique<EditorShell>();
            shell->SetPreferences(pages, {});
            Configure(*shell, layout, panels);
            auto *dialog = Open(*shell);
            ASSERT_NE(dialog, nullptr);
            Edit(*dialog, "value", int32_t{7});
            int notifications = 0;
            dialog->History().AddChangeCallback([&] {
                if (++notifications == deleteAt)
                    shell.reset();
            });
            shell->OpenPreferences();
            EXPECT_EQ(notifications, deleteAt);
            EXPECT_EQ(shell, nullptr);
            EXPECT_EQ(live.value, 3);
        }
    }

    TEST_F(EditorPreferenceIntegrationTest, RealStorageFailureKeepsTheDialogHistoryAndAppliedScaleAvailableForRetry)
    {
        GeneralSettings    general;
        EditorViewSettings editor;
        RenderingSettings  rendering;
        LayoutModel        layout;
        PanelRegistry      panels;
        EditorShell        shell;
        shell.SetUiScale(2.0f);
        BuiltinPreferencePages host(*pages);
        ASSERT_TRUE(host.Bind(general, editor, rendering,
                              [&] {
                                  shell.SetUserUiScale(editor.uiScale);
                                  return PreferenceResult{};
                              })
                        .Succeeded());
        const auto blocker = root / "blocked";
        {
            std::ofstream file(blocker);
            file << "preserve";
        }
        auto persistence = std::make_shared<PreferencesPersistence>(std::make_shared<FilePreferenceStorage>(blocker / "editor-preferences.json"));
        shell.SetPreferences(pages, {});
        shell.SetPreferencesPersistence(persistence);
        Configure(shell, layout, panels);
        auto *dialog = Open(shell);
        ASSERT_NE(dialog, nullptr);
        dialog->SetPageIndex(1);
        Edit(*dialog, "uiScale", 1.25f);
        dialog->Apply();
        EXPECT_EQ(dialog->GetLastApplyResult().code, PreferenceCode::ReadFailed);
        EXPECT_FALSE(dialog->GetLastApplyResult().HasPartialEffects());
        EXPECT_TRUE(dialog->IsOpen());
        EXPECT_TRUE(dialog->History().CanUndo());
        EXPECT_FLOAT_EQ(editor.uiScale, 1.0f);
        EXPECT_FLOAT_EQ(shell.GetSurfaceUiScale(), 2.0f);
        std::filesystem::remove(blocker);
        dialog->Apply();
        ASSERT_TRUE(dialog->GetLastApplyResult().Succeeded()) << dialog->GetLastApplyResult().message;
        EXPECT_FALSE(dialog->History().CanUndo());
        EXPECT_FLOAT_EQ(editor.uiScale, 1.25f);
        EXPECT_FLOAT_EQ(shell.GetSurfaceUiScale(), 2.5f);
        shell.SetPreferences(nullptr, {});
    }
} // namespace sky::editor::editor_preference_integration_test

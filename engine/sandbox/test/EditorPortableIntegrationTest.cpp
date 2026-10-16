#include <editor/core/EditorCore.h>
#include <editor/core/command/CommandService.h>
#include <editor/core/extension/EditorActionRegistry.h>
#include <editor/core/extension/EditorScopeStack.h>
#include <editor/core/input/KeyModifiers.h>
#include <editor/core/preferences/EditorSettings.h>
#include <editor/core/preferences/PreferencePageRegistry.h>
#include <editor/shell/EditorShell.h>
#include <editor/shell/PreferencesDialog.h>
#include <editor/widgets/ReflectedFormView.h>
#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>

namespace sky::editor {
    namespace {
        class PortableValueCommand : public UndoCommand {
        public:
            explicit PortableValueCommand(int &value) : value(value)
            {
            }
            void Do() override
            {
                value = 9;
            }
            void Undo() override
            {
                value = 0;
            }

        private:
            int &value;
        };

        ReflectedFormView *PortableForm(sky::ui::UIElement &element)
        {
            if (auto *form = dynamic_cast<ReflectedFormView *>(&element)) {
                return form;
            }
            for (const auto &child : element.GetChildren()) {
                if (auto *form = PortableForm(*child)) {
                    return form;
                }
            }
            return nullptr;
        }

        PropertyField *AutosaveField(ReflectedFormView &form)
        {
            for (auto &section : form.Form().GetSections()) {
                for (auto &field : section.fields) {
                    if (field.path == "autosave") {
                        return &field;
                    }
                }
            }
            return nullptr;
        }

        sky::ui::UIKeyEvent PortableKey(uint32_t key, uint32_t modifiers = 0, sky::ui::UIKeyAction action = sky::ui::UIKeyAction::DOWN)
        {
            sky::ui::UIKeyEvent event;
            event.keyCode   = key;
            event.modifiers = modifiers;
            event.action    = action;
            return event;
        }

        class EditorPortableIntegrationTest : public testing::Test {
        protected:
            void SetUp() override
            {
                RegisterEditorSettings(sky::SerializationContext::Get());
                PreferencePageRegistry::Get()->Clear();
                EditorActionRegistry::Get()->Clear();
                EditorScopeStack::Get()->Clear();
                EditorCore::GetCommandService().Clear();
            }
            void TearDown() override
            {
                EditorActionRegistry::Get()->Clear();
                EditorScopeStack::Get()->Clear();
                PreferencePageRegistry::Get()->Clear();
                EditorCore::GetCommandService().Clear();
            }
        };
    } // namespace

    TEST_F(EditorPortableIntegrationTest, PreferencesOwnsShellKeysUntilCancelThenGlobalHistoryResumes)
    {
        GeneralSettings settings;
        PreferencePageRegistry::Get()->Add({"portable", "Portable", &settings, sky::GetTypeNode(sky::TypeInfo<GeneralSettings>::RegisteredId())});
        int   worldValue    = 0;
        auto &worldCommands = EditorCore::GetCommandService();
        worldCommands.Execute(std::make_unique<PortableValueCommand>(worldValue));
        int          saves = 0;
        int          plays = 0;
        EditorAction undo;
        undo.id          = "portable.undo";
        undo.accelerator = "Ctrl+Z";
        undo.invoke      = [&]() { worldCommands.Undo(); };
        EditorActionRegistry::Get()->Add(std::move(undo));
        EditorAction redo;
        redo.id          = "portable.redo";
        redo.accelerator = "Ctrl+Y";
        redo.invoke      = [&]() { worldCommands.Redo(); };
        EditorActionRegistry::Get()->Add(std::move(redo));
        EditorAction save;
        save.id          = "portable.save";
        save.accelerator = "Ctrl+S";
        save.invoke      = [&]() { ++saves; };
        EditorActionRegistry::Get()->Add(std::move(save));
        EditorAction play;
        play.id     = "play.play";
        play.invoke = [&]() { ++plays; };
        EditorActionRegistry::Get()->Add(std::move(play));

        LayoutModel layout;
        layout.SetDefault({"main"});
        PanelRegistry panels;
        panels.Register(PanelInfo{"main", "Main", 0.0f, 0.0f, nullptr});
        EditorShell shell;
        shell.SetLayout(&layout);
        shell.SetPanelRegistry(&panels);
        shell.Rebuild();
        shell.Layout(1000.0f, 700.0f);
        shell.SetPreferences(PreferencePageRegistry::Get(), []() {});
        shell.OpenPreferences();
        auto *dialog = dynamic_cast<PreferencesDialog *>(shell.HitTest(1.0f, 1.0f));
        ASSERT_NE(dialog, nullptr);
        auto *form = PortableForm(*dialog);
        ASSERT_NE(form, nullptr);
        ASSERT_NE(AutosaveField(*form), nullptr);
        ASSERT_NE(&form->Commands(), &worldCommands);
        ASSERT_TRUE(form->Form().Edit(*AutosaveField(*form), sky::Any(false), form->Commands()));
        EXPECT_TRUE(settings.autosave);

        ASSERT_TRUE(shell.DispatchKey(PortableKey('Z', kModCtrl)));
        EXPECT_TRUE(*AutosaveField(*form)->descriptor.GetValue().GetAsConst<bool>());
        EXPECT_EQ(form->Commands().GetUndoCount(), 0u);
        EXPECT_EQ(worldCommands.GetUndoCount(), 1u);
        EXPECT_EQ(worldValue, 9);
        ASSERT_TRUE(shell.DispatchKey(PortableKey('Y', kModCtrl)));
        EXPECT_FALSE(*AutosaveField(*form)->descriptor.GetValue().GetAsConst<bool>());
        EXPECT_EQ(form->Commands().GetUndoCount(), 1u);
        EXPECT_EQ(worldCommands.GetUndoCount(), 1u);

        EXPECT_TRUE(shell.DispatchKey(PortableKey('Z', kModCtrl, sky::ui::UIKeyAction::UP)));
        EXPECT_EQ(form->Commands().GetUndoCount(), 1u);
        EXPECT_TRUE(shell.DispatchKey(PortableKey('S', kModCtrl)));
        EXPECT_TRUE(shell.DispatchKey(PortableKey(0x74)));
        EXPECT_EQ(saves, 0);
        EXPECT_EQ(plays, 0);

        EXPECT_TRUE(shell.DispatchKey(PortableKey(0x1B)));
        EXPECT_FALSE(shell.IsPreferencesOpen());
        EXPECT_TRUE(settings.autosave);
        EXPECT_EQ(worldCommands.GetUndoCount(), 1u);
        EXPECT_TRUE(shell.DispatchKey(PortableKey('Z', kModCtrl)));
        EXPECT_EQ(worldValue, 0);
        EXPECT_EQ(worldCommands.GetUndoCount(), 0u);
    }
} // namespace sky::editor

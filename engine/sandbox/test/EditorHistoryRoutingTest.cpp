// The actual shell accelerator/builder paths must share the service's
// cancel-before-history ordering, including document-local injected histories.
#include <core/environment/Once.h>
#include <core/type/TypeInfo.h>
#include <editor/core/command/CommandService.h>
#include <editor/core/extension/EditorActionRegistry.h>
#include <editor/core/extension/EditorScopeStack.h>
#include <editor/core/input/KeyModifiers.h>
#include <editor/core/property/PropertyChangeNotifier.h>
#include <editor/shell/EditorShell.h>
#include <editor/widgets/ActionBarBuilder.h>
#include <editor/widgets/ReflectedFormView.h>
#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>

namespace sky::editor::history_routing_test {
    struct Color {
        float r = 0.2f;
        float g = 0.4f;
        float b = 0.6f;
        float a = 1.0f;
    };
    struct Object {
        Color color;
    };

    namespace {
        const TypeNode *RegisterTypes()
        {
            if (Once::Get()->Run("EditorHistoryRoutingTest.Types")) {
                SerializationContext::Get()
                    ->Register<Color>("HistoryRoutingColor")
                    .Member<&Color::r>("r")
                    .Member<&Color::g>("g")
                    .Member<&Color::b>("b")
                    .Member<&Color::a>("a");
                SerializationContext::Get()->Register<Object>("HistoryRoutingObject").Member<&Object::color>("color");
            }
            return GetTypeNode(TypeInfo<Object>::RegisteredId());
        }
        PropertyField &ColorField(ReflectedFormView &view)
        {
            return view.Form().GetSections().front().fields.front();
        }
        ui::UIPointerEvent Pointer(ui::UIPointerAction action, float x, float y)
        {
            ui::UIPointerEvent event;
            event.action = action;
            event.x      = x;
            event.y      = y;
            return event;
        }
        void OpenPreview(ReflectedFormView &view)
        {
            view.SynchronizeLayout();
            view.Arrange({0.0f, 0.0f, 800.0f, 600.0f});
            auto widget = view.Widgets().FindShared(PropertyEditorKind::Color);
            ASSERT_NE(widget, nullptr);
            ASSERT_TRUE(widget->OnDown(view, ColorField(view), Pointer(ui::UIPointerAction::DOWN, 101.0f, 35.0f), {100.0f, 30.0f, 400.0f, 60.0f}));
            view.OnPointerEvent(Pointer(ui::UIPointerAction::DOWN, 312.0f, 144.0f));
            ASSERT_TRUE(view.WantsPointerCapture());
        }
        void FinishOldInteraction(ReflectedFormView &view)
        {
            ui::UIKeyEvent escape;
            escape.keyCode = 0x1B;
            view.OnKeyEvent(escape);
            view.OnPointerEvent(Pointer(ui::UIPointerAction::UP, 312.0f, 144.0f));
            view.SynchronizeLayout();
            EXPECT_FALSE(view.WantsPointerCapture());
        }

        enum class Entry { Shortcut, Menu, ToolBar, External };
        void Invoke(Entry entry, bool redo, EditorShell &shell, CommandService &commands)
        {
            if (entry == Entry::External) {
                EXPECT_TRUE(redo ? commands.Redo() : commands.Undo());
                return;
            }
            if (entry == Entry::Shortcut) {
                ui::UIKeyEvent key;
                key.keyCode   = redo ? 0x59 : 0x5A;
                key.modifiers = kModCtrl;
                EXPECT_TRUE(shell.DispatchKey(key));
                return;
            }
            const ActionBarBuilder::Invoker invoke = [&shell](const std::string &id) { EXPECT_TRUE(shell.InvokeAction(id)); };
            if (entry == Entry::Menu) {
                const auto menus = ActionBarBuilder::BuildMenus(invoke);
                ASSERT_EQ(menus.size(), 1u);
                ASSERT_EQ(menus.front().items.size(), 2u);
                menus.front().items[redo ? 1 : 0].action();
            } else {
                const auto items = ActionBarBuilder::BuildToolBarItems(invoke);
                ASSERT_EQ(items.size(), 2u);
                items[redo ? 1 : 0].action();
            }
        }
        void RegisterActions(CommandService &commands)
        {
            auto *registry = EditorActionRegistry::Get();
            registry->Clear();
            EditorScopeStack::Get()->Clear();
            for (bool redo : {false, true}) {
                EditorAction action;
                action.id          = redo ? "history.redo" : "history.undo";
                action.label       = redo ? "Redo" : "Undo";
                action.group       = EditorGroup::History;
                action.menu        = EditorMenu::Edit;
                action.order       = redo ? 1 : 0;
                action.accelerator = redo ? "Ctrl+Y" : "Ctrl+Z";
                action.invoke      = [&commands, redo]() { EXPECT_TRUE(redo ? commands.Redo() : commands.Undo()); };
                registry->Add(std::move(action));
            }
        }
    } // namespace

    TEST(EditorHistoryRoutingTest, ShellShortcutMenuToolbarAndExternalHistoryCancelLivePreviews)
    {
        const auto *type = RegisterTypes();
        for (Entry entry : {Entry::Shortcut, Entry::Menu, Entry::ToolBar, Entry::External}) {
            SCOPED_TRACE(static_cast<int>(entry));
            CommandService         commands;
            PropertyEditorRegistry registry;
            PropertyChangeNotifier changes;
            const UiTheme          theme = MakeDarkTheme();
            registry.RegisterType(TypeInfo<Color>::RegisteredId(), PropertyEditorKind::Color);
            Object            object;
            EditorShell       shell;
            ReflectedFormView view(nullptr, "", {commands, registry, changes, theme});
            view.Bind({&object, type});
            Color committed;
            committed.r = 0.8f;
            committed.g = 0.2f;
            committed.b = 0.1f;
            ASSERT_TRUE(view.Form().Edit(ColorField(view), Any(committed), commands));
            RegisterActions(commands);

            OpenPreview(view);
            Invoke(entry, false, shell, commands);
            FinishOldInteraction(view);
            EXPECT_FLOAT_EQ(object.color.r, 0.2f);
            EXPECT_FLOAT_EQ(object.color.g, 0.4f);
            EXPECT_FLOAT_EQ(object.color.b, 0.6f);
            EXPECT_EQ(commands.GetUndoCount(), 0u);
            EXPECT_EQ(commands.GetRedoCount(), 1u);

            OpenPreview(view);
            Invoke(entry, true, shell, commands);
            FinishOldInteraction(view);
            EXPECT_FLOAT_EQ(object.color.r, committed.r);
            EXPECT_FLOAT_EQ(object.color.g, committed.g);
            EXPECT_FLOAT_EQ(object.color.b, committed.b);
            EXPECT_EQ(commands.GetUndoCount(), 1u);
            EXPECT_EQ(commands.GetRedoCount(), 0u);
            EditorActionRegistry::Get()->Clear();
        }
    }
} // namespace sky::editor::history_routing_test

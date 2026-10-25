#include "ReflectionTestEnvironment.h"
#include <editor/core/EditorCore.h>
#include <editor/core/input/KeyModifiers.h>
#include <editor/core/preferences/PreferencesSession.h>
#include <editor/core/property/PropertyDescriptor.h>
#include <editor/shell/PreferencesDialog.h>
#include <editor/widgets/ReflectedFormView.h>
#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace sky::editor::preferences_session_test {
    struct Small {
        int32_t value = 1;
    };
    struct Deep {
        std::string          path = "original";
        std::vector<int32_t> values{1, 2, 3};
        int32_t              hidden   = 41;
        int32_t              readOnly = 11;
        const int32_t        fixed    = 17;
    };
    struct Reentrant {
        int32_t               value = 1;
        std::function<void()> changed;
        void                  SetValue(const int32_t &next)
        {
            value = next;
        }
        int32_t GetValue() const
        {
            return value;
        }
        void Changed()
        {
            if (changed) {
                changed();
            }
        }
    };

    namespace {
        const auto *reflectionEnvironment = sky::editor::test::AddReflectionEnvironment([] {
            auto *context = SerializationContext::Get();
            context->Register<Small>("PreferencesSessionSmall").Member<&Small::value>("value");
            context->Register<Deep>("PreferencesSessionDeep")
                .Member<&Deep::path>("path")
                .Member<&Deep::values>("values")
                .Member<&Deep::readOnly>("readOnly")
                .Property(CommonPropertyKey::READONLY, Any(true))
                .Member<&Deep::fixed>("fixed");
            context->Register<Reentrant>("PreferencesSessionReentrant").MemberNoSetter<&Reentrant::GetValue, &Reentrant::Changed>("value");
            // This page deliberately has an ordinary setter plus a
            // notification, matching plugin-reflected settings.
            auto *member     = GetTypeMember("value", TypeInfo<Reentrant>::RegisteredId());
            member->setterFn = &Setter<Reentrant, int32_t, &Reentrant::SetValue>;
        });

        class PreferencesSessionTest : public testing::Test {
        protected:
            void SetUp() override
            {
                pages = PreferencePageRegistry::Get();
                pages->Clear();
                EditorCore::GetCommandService().Clear();
            }
            void TearDown() override
            {
                pages->Clear();
                EditorCore::GetCommandService().Clear();
            }
            template <class T>
            void Add(const std::string &id, T &object)
            {
                pages->Add({id, id, &object, GetTypeNode(TypeInfo<T>::RegisteredId())});
            }
            static const PreferencePage *Page(PreferencesSession &session, const std::string &id)
            {
                for (std::size_t i = 0; i < session.GetPageCount(); ++i) {
                    if (session.GetPage(i)->id == id) {
                        return session.GetPage(i);
                    }
                }
                return nullptr;
            }
            static PropertyDescriptor Property(const PreferencePage &page, const std::string &name)
            {
                return {page.object, &page.type->members.at(name), name, ""};
            }
            template <class T>
            static void Edit(PreferencesSession &session, const std::string &id, const std::string &name, const T &value)
            {
                const auto *page = Page(session, id);
                ASSERT_NE(page, nullptr);
                session.History().Execute(Property(*page, name).MakeEditCommand(Any(value)));
            }
            static ReflectedFormView &Form(PreferencesDialog &dialog)
            {
                const auto find = [](auto &self, ui::UIElement &element) -> ReflectedFormView * {
                    if (auto *view = dynamic_cast<ReflectedFormView *>(&element)) {
                        return view;
                    }
                    for (const auto &child : element.GetChildren()) {
                        if (auto *view = self(self, *child)) {
                            return view;
                        }
                    }
                    return nullptr;
                };
                return *find(find, dialog);
            }
            static void EditDialog(PreferencesDialog &dialog, int32_t value)
            {
                auto &view  = Form(dialog);
                auto &field = view.Form().GetSections().front().fields.front();
                ASSERT_TRUE(view.Form().Edit(field, Any(value), dialog.History()));
            }
            static void Click(PreferencesDialog &dialog, int button)
            {
                if (dialog.GetBounds().Width() <= 0.0f || dialog.GetBounds().Height() <= 0.0f) {
                    dialog.SetBounds({0.0f, 0.0f, 1000.0f, 800.0f});
                }
                const auto         rect = dialog.ButtonRect(button);
                ui::UIPointerEvent event;
                event.action = ui::UIPointerAction::DOWN;
                event.x      = (rect.left + rect.right) * 0.5f;
                event.y      = (rect.top + rect.bottom) * 0.5f;
                dialog.OnPointerEvent(event);
            }
            PreferencePageRegistry *pages = nullptr;
        };
    } // namespace

    TEST_F(PreferencesSessionTest, CancelDiscardsDeepValuesAndLeavesGlobalHistoryAlone)
    {
        Deep live;
        Add("deep", live);
        PreferencesSession session;
        session.Begin(pages);
        auto *working = static_cast<Deep *>(Page(session, "deep")->object);
        EXPECT_NE(working, &live);
        EXPECT_NE(working->values.data(), live.values.data());
        Small          unrelated;
        PreferencePage global{"global", "global", &unrelated, GetTypeNode(TypeInfo<Small>::RegisteredId())};
        EditorCore::GetCommandService().Execute(Property(global, "value").MakeEditCommand(Any(int32_t{9})));
        Edit(session, "deep", "path", std::string(200, 'x'));
        Edit(session, "deep", "values", std::vector<int32_t>{8, 9});
        EXPECT_EQ(live.path, "original");
        EXPECT_EQ(live.values, (std::vector<int32_t>{1, 2, 3}));
        EXPECT_TRUE(session.History().Undo());
        EXPECT_TRUE(session.History().Redo());
        session.End();
        EXPECT_FALSE(session.History().Undo());
        EXPECT_FALSE(session.History().Redo());
        EXPECT_EQ(live.path, "original");
        EXPECT_EQ(EditorCore::GetCommandService().GetUndoCount(), 1u);
        EXPECT_TRUE(EditorCore::GetCommandService().Undo());
        EXPECT_EQ(unrelated.value, 1);
    }

    TEST_F(PreferencesSessionTest, ApplyBecomesBaselineAndReopenStartsNewHistory)
    {
        Deep live;
        Add("deep", live);
        PreferencesSession session;
        session.Begin(pages);
        Edit(session, "deep", "path", std::string("applied"));
        Edit(session, "deep", "values", std::vector<int32_t>{4, 5});
        live.hidden = 99; // unreflected live state is never overwritten
        ASSERT_TRUE(session.Apply());
        EXPECT_EQ(live.path, "applied");
        EXPECT_EQ(live.values, (std::vector<int32_t>{4, 5}));
        EXPECT_EQ(live.hidden, 99);
        EXPECT_FALSE(session.History().CanUndo());
        Edit(session, "deep", "path", std::string("discarded"));
        session.End();
        EXPECT_EQ(live.path, "applied");
        session.Begin(pages);
        EXPECT_EQ(static_cast<Deep *>(Page(session, "deep")->object)->path, "applied");
        EXPECT_FALSE(session.History().CanUndo());
        EXPECT_FALSE(session.History().CanRedo());
    }

    TEST_F(PreferencesSessionTest, ApplyPreservesConstAndMetadataReadOnlyLiveMembers)
    {
        Deep live;
        Add("deep", live);
        PreferencesSession session;
        session.Begin(pages);
        auto *working     = static_cast<Deep *>(Page(session, "deep")->object);
        working->readOnly = 88; // even an out-of-band working mutation cannot write this field
        live.readOnly     = 44;
        Edit(session, "deep", "path", std::string("editable"));
        ASSERT_TRUE(session.Apply());
        EXPECT_EQ(live.path, "editable");
        EXPECT_EQ(live.readOnly, 44);
        EXPECT_EQ(live.fixed, 17);
    }

    TEST_F(PreferencesSessionTest, InlineWorkingObjectsAndCrossPageHistorySurviveReorder)
    {
        std::vector<Small> live(40);
        for (std::size_t i = 0; i < live.size(); ++i) {
            Add(std::to_string(i), live[i]);
        }
        PreferencesSession session;
        session.Begin(pages);
        const auto *first = Page(session, "0")->object;
        const auto *last  = Page(session, "39")->object;
        Edit(session, "0", "value", int32_t{7});
        Edit(session, "39", "value", int32_t{8});
        ASSERT_TRUE(pages->Move("39", 0));
        EXPECT_FALSE(session.MatchesRegistry());
        session.Synchronize();
        EXPECT_EQ(Page(session, "0")->object, first);
        EXPECT_EQ(Page(session, "39")->object, last);
        EXPECT_EQ(session.History().GetUndoCount(), 2u);
        EXPECT_TRUE(session.History().Undo());
        EXPECT_EQ(static_cast<const Small *>(last)->value, 1);
        EXPECT_TRUE(session.History().Undo());
        EXPECT_EQ(static_cast<const Small *>(first)->value, 1);
        EXPECT_TRUE(session.History().Redo());
        EXPECT_EQ(static_cast<const Small *>(first)->value, 7);
        ASSERT_TRUE(session.Apply());
        EXPECT_EQ(live.front().value, 7);
        EXPECT_EQ(live.back().value, 1);
    }

    TEST_F(PreferencesSessionTest, RemovedReplacedAndSameAddressRegistrationsInvalidateHistory)
    {
        Small live;
        Add("page", live);
        PreferencesSession session;
        session.Begin(pages);
        Edit(session, "page", "value", int32_t{7});
        pages->Clear();
        live.value = 33;
        Add("page", live); // ABA: same id, address and type, new registration
        EXPECT_FALSE(session.Apply());
        session.Synchronize();
        EXPECT_FALSE(session.History().CanUndo());
        EXPECT_EQ(static_cast<Small *>(Page(session, "page")->object)->value, 33);
        Deep replacement;
        ASSERT_TRUE(pages->Remove("page"));
        Add("page", replacement);
        session.Synchronize();
        EXPECT_EQ(Page(session, "page")->type, GetTypeNode(TypeInfo<Deep>::RegisteredId()));
        EXPECT_EQ(static_cast<Deep *>(Page(session, "page")->object)->path, "original");
        pages->Remove("page");
        session.Synchronize();
        EXPECT_EQ(session.GetPageCount(), 0u);
        EXPECT_FALSE(session.History().Undo());
    }

    TEST_F(PreferencesSessionTest, RegistryMutationDuringClearIsRecollectedAndReentrantBeginWins)
    {
        Small oldLive;
        Small nextLive;
        nextLive.value = 21;
        Add("old", oldLive);
        PreferencesSession session;
        session.Begin(pages);
        bool       armed    = true;
        const auto callback = session.History().AddChangeCallback([&] {
            if (!armed) {
                return;
            }
            armed = false;
            pages->Clear();
            Add("next", nextLive);
        });
        pages->Remove("old");
        session.Synchronize();
        ASSERT_NE(Page(session, "next"), nullptr);
        EXPECT_EQ(static_cast<Small *>(Page(session, "next")->object)->value, 21);
        session.History().RemoveChangeCallback(callback);
        armed              = true;
        const auto restart = session.History().AddChangeCallback([&] {
            if (!armed) {
                return;
            }
            armed = false;
            session.Begin(pages);
            Edit(session, "next", "value", int32_t{42});
        });
        session.End();
        EXPECT_TRUE(session.IsOpen());
        EXPECT_EQ(session.History().GetUndoCount(), 1u);
        EXPECT_EQ(static_cast<Small *>(Page(session, "next")->object)->value, 42);
        session.History().RemoveChangeCallback(restart);
    }

    TEST_F(PreferencesSessionTest, SetterNotificationCannotClearReenteredSessionHistory)
    {
        Reentrant live;
        Small     next;
        Add("first", live);
        PreferencesSession session;
        session.Begin(pages);
        Edit(session, "first", "value", int32_t{7});
        live.changed = [&] {
            session.End();
            pages->Clear();
            Add("next", next);
            session.Begin(pages);
            Edit(session, "next", "value", int32_t{99});
        };
        EXPECT_FALSE(session.Apply());
        EXPECT_EQ(live.value, 7);
        EXPECT_EQ(session.History().GetUndoCount(), 1u);
        EXPECT_EQ(static_cast<Small *>(Page(session, "next")->object)->value, 99);
    }

    TEST_F(PreferencesSessionTest, DialogUsesLocalHistoryAcrossPagesAndClosesByDiscarding)
    {
        Small first;
        Small second;
        Add("first", first);
        Add("second", second);
        PreferencesDialog dialog(nullptr);
        dialog.SetModel(pages);
        dialog.Open();
        EditDialog(dialog, 7);
        dialog.SetPageIndex(1);
        EditDialog(dialog, 8);
        EXPECT_EQ(dialog.History().GetUndoCount(), 2u);
        EXPECT_EQ(first.value, 1);
        EXPECT_EQ(second.value, 1);
        ASSERT_TRUE(dialog.History().Undo());
        dialog.Apply();
        EXPECT_EQ(first.value, 7);
        EXPECT_EQ(second.value, 1);
        EXPECT_FALSE(dialog.History().CanUndo());
        EditDialog(dialog, 9);
        dialog.Close();
        EXPECT_EQ(second.value, 1);
        EXPECT_FALSE(dialog.History().Undo());
        dialog.Open();
        EXPECT_EQ(dialog.GetPageIndex(), 0);
        EXPECT_EQ(dialog.History().GetUndoCount(), 0u);
    }

    TEST_F(PreferencesSessionTest, ApplyClearCannotConsumeReplacementSessionHistory)
    {
        Small live;
        Add("page", live);
        PreferencesSession session;
        session.Begin(pages);
        Edit(session, "page", "value", int32_t{7});
        bool       armed    = true;
        const auto callback = session.History().AddChangeCallback([&] {
            if (!armed) {
                return;
            }
            armed = false;
            session.Begin(pages);
            Edit(session, "page", "value", int32_t{99});
        });
        EXPECT_FALSE(session.Apply());
        EXPECT_EQ(live.value, 7);
        EXPECT_EQ(session.History().GetUndoCount(), 1u);
        EXPECT_EQ(static_cast<Small *>(Page(session, "page")->object)->value, 99);
        session.History().RemoveChangeCallback(callback);
    }

    TEST_F(PreferencesSessionTest, DialogRegistryReplacementAndReorderRebindSafely)
    {
        Small first;
        Small second;
        Add("first", first);
        Add("second", second);
        PreferencesDialog dialog(nullptr);
        dialog.SetModel(pages);
        dialog.Open();
        EditDialog(dialog, 7);
        pages->Move("first", 1);
        dialog.Arrange({0, 0, 1000, 700});
        EXPECT_EQ(dialog.GetPageIndex(), 1);
        ASSERT_TRUE(dialog.History().Undo());
        ASSERT_TRUE(dialog.History().Redo());
        pages->Clear();
        first.value = 33;
        Add("first", first);
        dialog.Apply();
        EXPECT_EQ(first.value, 33);
        EXPECT_FALSE(dialog.History().CanUndo());
        EXPECT_EQ(dialog.GetPageCount(), 1);
    }

    TEST_F(PreferencesSessionTest, AppliedCallbacksMayReopenReplaceOrDestroyDialog)
    {
        Small live;
        Add("page", live);
        auto dialog = std::make_unique<PreferencesDialog>(nullptr);
        dialog->SetModel(pages);
        dialog->Open();
        EditDialog(*dialog, 7);
        int closes = 0;
        dialog->SetOnClosed([&] { ++closes; });
        dialog->SetOnApplied([&] {
            pages->Clear();
            live.value = 13;
            Add("page", live);
            dialog->Open();
        });
        Click(*dialog, 0); // OK must not close the callback's new session
        ASSERT_NE(dialog, nullptr);
        EXPECT_TRUE(dialog->IsOpen());
        EXPECT_EQ(closes, 0);
        EXPECT_EQ(live.value, 13);
        dialog->SetOnApplied([&] { dialog.reset(); });
        dialog->Apply();
        EXPECT_EQ(dialog, nullptr);
        dialog = std::make_unique<PreferencesDialog>(nullptr);
        dialog->SetModel(pages);
        dialog->Open();
        dialog->SetOnClosed([&] { dialog.reset(); });
        dialog->Close();
        EXPECT_EQ(dialog, nullptr);
    }

    TEST_F(PreferencesSessionTest, HistoryNotificationsMayDestroyDialogAtTransitionsAndKeyDispatch)
    {
        Small live;
        Add("page", live);
        for (int action = 0; action < 5; ++action) {
            auto dialog = std::make_unique<PreferencesDialog>(nullptr);
            dialog->SetModel(pages);
            dialog->Open();
            EditDialog(*dialog, 7);
            dialog->History().AddChangeCallback([&] { dialog.reset(); });
            if (action == 0) {
                dialog->Open();
            } else if (action == 1) {
                dialog->Close();
            } else if (action == 2) {
                dialog->Apply();
            } else if (action == 3) {
                ui::UIKeyEvent undo;
                undo.action    = ui::UIKeyAction::DOWN;
                undo.modifiers = kModCtrl;
                undo.keyCode   = 0x5A;
                dialog->OnKeyEvent(undo);
            } else {
                Click(*dialog, 3); // Reset must stop after an observer deletes its form
            }
            EXPECT_EQ(dialog, nullptr);
        }
    }

    TEST_F(PreferencesSessionTest, CloseRequestedByEditObserverWaitsForWidgetEventReturn)
    {
        Small live;
        Add("page", live);
        PreferencesDialog dialog(nullptr);
        dialog.SetModel(pages);
        dialog.Open();
        bool requested = false;
        dialog.History().AddChangeCallback([&] {
            if (!requested) {
                requested = true;
                dialog.Close();
                EXPECT_TRUE(dialog.IsOpen());
            }
        });
        EditDialog(dialog, 7);
        EXPECT_TRUE(dialog.IsOpen());
        dialog.Arrange({0, 0, 1000, 700});
        EXPECT_FALSE(dialog.IsOpen());
        EXPECT_EQ(live.value, 1);
        EXPECT_FALSE(dialog.History().CanUndo());
    }
} // namespace sky::editor::preferences_session_test

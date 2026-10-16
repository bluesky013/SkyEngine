#include <core/environment/Once.h>
#include <editor/core/preferences/PreferencesPersistence.h>
#include <editor/core/preferences/PreferencesSession.h>
#include <editor/core/property/PropertyDescriptor.h>
#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>

#include <functional>
#include <string>
#include <vector>

namespace sky::editor::preferences_copy_lifetime_test {

    struct CopySettings {
        CopySettings() = default;
        CopySettings(const CopySettings &other)
        {
            const auto callback = onCopy;
            if (callback) {
                callback(other);
            }
            // Read owning data after reentry, so ASan checks that the copy's
            // source survives a callback that releases its former owner.
            payload = other.payload;
            value   = other.value;
        }
        CopySettings &operator=(const CopySettings &) = default;

        std::string                                      payload = std::string(180, 'p');
        int32_t                                          value   = 1;
        static std::function<void(const CopySettings &)> onCopy;
    };

    std::function<void(const CopySettings &)> CopySettings::onCopy;

    namespace {
        class PreferencesCopyLifetimeTest : public testing::Test {
        protected:
            void SetUp() override
            {
                if (Once::Get()->Run("PreferencesCopyLifetimeTest.Types")) {
                    SerializationContext::Get()
                        ->Register<CopySettings>("PreferencesCopyLifetimeSettings")
                        .Member<&CopySettings::value>("value")
                        .Member<&CopySettings::payload>("payload");
                }
                CopySettings::onCopy = {};
                pages                = PreferencePageRegistry::Get();
                pages->Clear();
            }
            void TearDown() override
            {
                CopySettings::onCopy = {};
                pages->Clear();
            }
            void Add(const std::string &id, CopySettings &value)
            {
                pages->Add({id, id, &value, GetTypeNode(TypeInfo<CopySettings>::RegisteredId())});
            }
            static void Edit(PreferencesSession &session, int32_t value)
            {
                const auto *page = session.GetPage(0);
                ASSERT_NE(page, nullptr);
                PropertyDescriptor descriptor{page->object, &page->type->members.at("value"), "value", ""};
                session.History().Execute(descriptor.MakeEditCommand(Any(value)));
            }
            static void ExpectWorking(const PreferencesSession &session, const std::string &id, int32_t value)
            {
                ASSERT_TRUE(session.IsOpen());
                ASSERT_EQ(session.GetPageCount(), 1u);
                const auto *page = session.GetPage(0);
                ASSERT_NE(page, nullptr);
                EXPECT_EQ(page->id, id);
                EXPECT_EQ(static_cast<const CopySettings *>(page->object)->value, value);
                EXPECT_EQ(static_cast<const CopySettings *>(page->object)->payload, std::string(180, 'p'));
            }
            PreferencePageRegistry *pages = nullptr;
        };
    } // namespace

    TEST_F(PreferencesCopyLifetimeTest, CaptureCopyClearingRegistryStopsBeforeTheNextPage)
    {
        CopySettings first;
        CopySettings second;
        Add("first", first);
        Add("second", second);
        int firstCopies      = 0;
        int secondCopies     = 0;
        CopySettings::onCopy = [&](const CopySettings &source) {
            if (&source == &first) {
                ++firstCopies;
                pages->Clear();
            } else if (&source == &second) {
                ++secondCopies;
            }
        };
        std::vector<PreferenceValue> values;
        const auto                   result = CapturePreferenceValues(pages->GetPages(), values, [&] { return pages->GetPages().size() == 2u; });
        EXPECT_EQ(result.code, PreferenceCode::Conflict);
        EXPECT_FALSE(result.HasPartialEffects());
        EXPECT_TRUE(values.empty());
        EXPECT_EQ(firstCopies, 1);
        EXPECT_EQ(secondCopies, 0);
    }

    TEST_F(PreferencesCopyLifetimeTest, WorkingCopyCanEndSessionAndContinueReadingItsOwnedSource)
    {
        CopySettings live;
        Add("original", live);
        PreferencesSession session;
        session.Begin(pages);
        Edit(session, 7);
        const auto *working  = static_cast<const CopySettings *>(session.GetPage(0)->object);
        bool        ended    = false;
        CopySettings::onCopy = [&](const CopySettings &source) {
            if (&source == working) {
                ended = true;
                session.End();
            }
        };
        const auto result = session.ApplyDetailed();
        EXPECT_EQ(result.code, PreferenceCode::Conflict);
        EXPECT_FALSE(result.HasPartialEffects());
        EXPECT_TRUE(ended);
        EXPECT_FALSE(session.IsOpen());
        EXPECT_EQ(session.GetPageCount(), 0u);
        EXPECT_FALSE(session.History().CanUndo());
        EXPECT_EQ(live.value, 1);
        EXPECT_EQ(live.payload, std::string(180, 'p'));
    }

    TEST_F(PreferencesCopyLifetimeTest, BeginCopyReentryPreservesTheReplacementSessionAndHistory)
    {
        CopySettings original;
        CopySettings replacement;
        replacement.value = 2;
        Add("original", original);
        PreferencesSession session;
        bool               replaced = false;
        CopySettings::onCopy        = [&](const CopySettings &source) {
            if (&source == &original && !replaced) {
                replaced = true;
                pages->Clear();
                Add("replacement", replacement);
                session.Begin(pages);
                Edit(session, 42);
            }
        };
        session.Begin(pages);
        EXPECT_TRUE(replaced);
        ExpectWorking(session, "replacement", 42);
        EXPECT_TRUE(session.MatchesRegistry());
        EXPECT_EQ(session.History().GetUndoCount(), 1u);
        ASSERT_TRUE(session.History().Undo());
        ExpectWorking(session, "replacement", 2);
    }

    TEST_F(PreferencesCopyLifetimeTest, SynchronizeCopyReentryDoesNotOverwriteANewlyBegunSession)
    {
        CopySettings original;
        CopySettings added;
        CopySettings replacement;
        replacement.value = 3;
        Add("original", original);
        PreferencesSession session;
        session.Begin(pages);
        Add("added", added);
        bool replaced        = false;
        CopySettings::onCopy = [&](const CopySettings &source) {
            if (&source == &added && !replaced) {
                replaced = true;
                pages->Clear();
                Add("replacement", replacement);
                session.Begin(pages);
                Edit(session, 99);
            }
        };
        session.Synchronize();
        EXPECT_TRUE(replaced);
        ExpectWorking(session, "replacement", 99);
        EXPECT_TRUE(session.MatchesRegistry());
        EXPECT_EQ(session.History().GetUndoCount(), 1u);
        ASSERT_TRUE(session.History().Undo());
        ExpectWorking(session, "replacement", 3);
    }

    TEST_F(PreferencesCopyLifetimeTest, SynchronizeCopyEditingExistingWorkKeepsItsPageAndNewHistory)
    {
        CopySettings original;
        CopySettings added;
        Add("original", original);
        PreferencesSession session;
        session.Begin(pages);
        Add("added", added);
        bool edited          = false;
        CopySettings::onCopy = [&](const CopySettings &source) {
            if (&source == &added) {
                edited = true;
                Edit(session, 17);
            }
        };
        session.Synchronize();
        EXPECT_TRUE(edited);
        ExpectWorking(session, "original", 17);
        EXPECT_EQ(session.History().GetUndoCount(), 1u);
        EXPECT_FALSE(session.MatchesRegistry());
        ASSERT_TRUE(session.History().Undo());
        ExpectWorking(session, "original", 1);
    }

} // namespace sky::editor::preferences_copy_lifetime_test

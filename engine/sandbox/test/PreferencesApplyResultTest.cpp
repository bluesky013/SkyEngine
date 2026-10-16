#include <core/environment/Once.h>
#include <editor/core/preferences/PreferencesPersistence.h>
#include <editor/core/preferences/PreferencesSession.h>
#include <editor/core/property/PropertyDescriptor.h>
#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>
#include <rapidjson/document.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace sky::editor::preferences_apply_result_test {

    struct Setting {
        int32_t               value        = 1;
        bool                  rejectWrites = false;
        std::function<void()> onSet;
    };

    struct ThrowingSettings {
        ThrowingSettings() = default;
        ThrowingSettings(const ThrowingSettings &other) : payload(other.payload), value(other.value)
        {
            if (value == 7) {
                throw std::runtime_error("injected working settings copy failure");
            }
        }
        ThrowingSettings &operator=(const ThrowingSettings &) = default;

        // A nontrivial heap-backed member also exercises cleanup after a copy
        // throws: its partially constructed string has already been destroyed.
        std::string payload = std::string(180, 'p');
        int32_t     value   = 1;
    };

    struct AccessorSettings {
        void SetValue(const int32_t &next)
        {
            value = next;
        }
        int32_t GetValue()
        {
            return value;
        }
        void SetPath(const std::string &next)
        {
            path = next;
        }
        std::string GetPath()
        {
            return path;
        }
        int32_t     value = 1;
        std::string path  = std::string(100, 'a');
    };

    namespace {
        class MemoryStorage : public IPreferenceStorage {
        public:
            PreferenceResult Read(std::string &document) override
            {
                ++reads;
                const auto callback = onRead;
                if (callback)
                    callback();
                if (failRead)
                    return {PreferenceCode::ReadFailed};
                if (!exists)
                    return {PreferenceCode::NotFound};
                document = bytes;
                return {};
            }
            PreferenceResult Write(const std::string &document) override
            {
                ++writes;
                if (failWrite)
                    return {PreferenceCode::WriteFailed};
                bytes               = document;
                exists              = true;
                const auto callback = onWrite;
                if (callback)
                    callback();
                return {};
            }
            bool                  exists    = true;
            bool                  failRead  = false;
            bool                  failWrite = false;
            int                   reads     = 0;
            int                   writes    = 0;
            std::string           bytes     = "{}";
            std::function<void()> onRead;
            std::function<void()> onWrite;
        };

        class PreferencesApplyResultTest : public testing::Test {
        protected:
            void SetUp() override
            {
                if (Once::Get()->Run("PreferencesApplyResultTest.Setting")) {
                    SerializationContext::Get()->Register<Setting>("PreferencesApplyResultSetting").Member<&Setting::value>("value");
                    SerializationContext::Get()
                        ->Register<ThrowingSettings>("PreferencesApplyResultThrowingSettings")
                        .Member<&ThrowingSettings::value>("value")
                        .Member<&ThrowingSettings::payload>("payload");
                    SerializationContext::Get()
                        ->Register<AccessorSettings>("PreferencesApplyResultAccessorSettings")
                        .Member<&AccessorSettings::SetValue, &AccessorSettings::GetValue>("value")
                        .Member<&AccessorSettings::SetPath, &AccessorSettings::GetPath>("path");
                    GetTypeMember("value", TypeInfo<Setting>::RegisteredId())->setterFn = [](void *object, const void *value) {
                        auto      &setting  = *static_cast<Setting *>(object);
                        const auto callback = setting.onSet;
                        if (callback)
                            callback();
                        if (setting.rejectWrites)
                            return false;
                        setting.value = *static_cast<const int32_t *>(value);
                        return true;
                    };
                }
                pages = PreferencePageRegistry::Get();
                pages->Clear();
                storage = std::make_shared<MemoryStorage>();
            }
            void TearDown() override
            {
                pages->Clear();
            }
            PreferencePage Page(Setting &live)
            {
                return {"plugin", "Plugin", &live, GetTypeNode(TypeInfo<Setting>::RegisteredId())};
            }
            static int32_t Working(const PreferencesSession &session)
            {
                const auto *page = session.GetPage(0);
                EXPECT_NE(page, nullptr);
                return page ? static_cast<const Setting *>(page->object)->value : 0;
            }
            static void Edit(PreferencesSession &session, int32_t value)
            {
                const auto *page = session.GetPage(0);
                ASSERT_NE(page, nullptr);
                PropertyDescriptor descriptor{page->object, &page->type->members.at("value"), "value", ""};
                session.History().Execute(descriptor.MakeEditCommand(Any(value)));
                ASSERT_EQ(Working(session), value);
            }
            static void ExpectNoEffects(const PreferenceResult &result)
            {
                EXPECT_FALSE(result.storageWritten);
                EXPECT_FALSE(result.liveWritten);
                EXPECT_FALSE(result.applicationApplied);
                EXPECT_FALSE(result.HasPartialEffects());
            }
            int32_t StoredValue() const
            {
                rapidjson::Document document;
                document.Parse(storage->bytes.c_str());
                if (document.HasParseError() || !document.IsObject() || !document.HasMember("plugin") || !document["plugin"].IsObject() ||
                    !document["plugin"].HasMember("elements") || !document["plugin"]["elements"].IsObject() ||
                    !document["plugin"]["elements"].HasMember("value") || !document["plugin"]["elements"]["value"].IsInt()) {
                    ADD_FAILURE() << "The persisted plugin value is not a typed integer";
                    return 0;
                }
                return document["plugin"]["elements"]["value"].GetInt();
            }
            PreferencePageRegistry        *pages = nullptr;
            std::shared_ptr<MemoryStorage> storage;
        };
    } // namespace

    TEST_F(PreferencesApplyResultTest, SuccessfulApplyValidatesPersistsPublishesAndThenClearsHistory)
    {
        Setting                  live;
        std::vector<std::string> events;
        auto                     page = Page(live);
        page.validate                 = [&events, &live](const PreferencePage &candidate) {
            events.push_back("validate");
            EXPECT_NE(candidate.object, &live);
            EXPECT_EQ(static_cast<const Setting *>(candidate.object)->value, 7);
            return PreferenceResult{};
        };
        page.apply = [&events, &live](const PreferencePage &applied) {
            events.push_back("apply");
            EXPECT_EQ(applied.object, &live);
            EXPECT_EQ(live.value, 7);
            return PreferenceResult{};
        };
        pages->Add(std::move(page));
        PreferencesSession session;
        session.Begin(pages);
        Edit(session, 7);
        storage->onWrite = [&events]() { events.push_back("write"); };
        live.onSet       = [&events]() { events.push_back("setter"); };
        PreferencesPersistence persistence(storage);
        const auto             result = session.ApplyDetailed(&persistence);
        ASSERT_TRUE(result.Succeeded()) << result.message;
        EXPECT_TRUE(result.storageWritten);
        EXPECT_TRUE(result.liveWritten);
        EXPECT_TRUE(result.applicationApplied);
        EXPECT_FALSE(result.HasPartialEffects());
        ASSERT_EQ(events.size(), 4u);
        EXPECT_EQ(events, (std::vector<std::string>{"validate", "write", "setter", "apply"}));
        EXPECT_EQ(live.value, 7);
        EXPECT_EQ(Working(session), 7);
        EXPECT_FALSE(session.History().CanUndo());
        session.End();
        EXPECT_EQ(live.value, 7);
    }

    TEST_F(PreferencesApplyResultTest, ThrowingWorkingCopyReturnsInvalidDataAndLeavesTheSessionAvailableForRetry)
    {
        ThrowingSettings live;
        pages->Add({"throwing", "Throwing", &live, GetTypeNode(TypeInfo<ThrowingSettings>::RegisteredId())});
        PreferencesSession session;
        session.Begin(pages);
        const auto editValue = [&session](int32_t value) {
            const auto *page = session.GetPage(0);
            ASSERT_NE(page, nullptr);
            PropertyDescriptor descriptor{page->object, &page->type->members.at("value"), "value", ""};
            session.History().Execute(descriptor.MakeEditCommand(Any(value)));
        };
        editValue(7);
        PreferencesPersistence persistence(storage);
        const auto             failed = session.ApplyDetailed(&persistence);
        EXPECT_EQ(failed.code, PreferenceCode::InvalidData);
        ExpectNoEffects(failed);
        EXPECT_EQ(storage->writes, 0);
        EXPECT_EQ(live.value, 1);
        ASSERT_NE(session.GetPage(0), nullptr);
        const auto *working = static_cast<const ThrowingSettings *>(session.GetPage(0)->object);
        EXPECT_EQ(working->value, 7);
        EXPECT_EQ(working->payload, live.payload);
        EXPECT_EQ(session.History().GetUndoCount(), 1u);
        std::vector<PreferenceValue> candidates;
        ASSERT_TRUE(CapturePreferenceValues(pages->GetPages(), candidates).Succeeded());
        ASSERT_EQ(candidates.size(), 1u);
        auto *candidate       = static_cast<ThrowingSettings *>(candidates.front().value.Data());
        candidate->value      = 7;
        const auto saveFailed = persistence.Save(candidates);
        EXPECT_EQ(saveFailed.code, PreferenceCode::InvalidData);
        ExpectNoEffects(saveFailed);
        EXPECT_EQ(storage->reads, 0);
        EXPECT_EQ(storage->writes, 0);
        EXPECT_EQ(candidate->payload, live.payload);
        candidate->value = 8;
        ASSERT_TRUE(persistence.Save(candidates).Succeeded());
        EXPECT_EQ(storage->writes, 1);
        EXPECT_EQ(live.value, 1);
        EXPECT_EQ(working->value, 7);
        EXPECT_EQ(session.History().GetUndoCount(), 1u);
        editValue(8);
        const auto retried = session.ApplyDetailed(&persistence);
        EXPECT_TRUE(retried.Succeeded()) << retried.message;
        EXPECT_EQ(live.value, 8);
        EXPECT_EQ(live.payload, std::string(180, 'p'));
        EXPECT_EQ(storage->writes, 2);
        EXPECT_FALSE(session.History().CanUndo());
    }

    TEST_F(PreferencesApplyResultTest, StorageFailuresKeepLiveWorkingAndHistoryAvailableForRetry)
    {
        Setting live;
        pages->Add(Page(live));
        PreferencesSession session;
        session.Begin(pages);
        Edit(session, 7);
        PreferencesPersistence persistence(storage);
        storage->failRead      = true;
        const auto readFailure = session.ApplyDetailed(&persistence);
        EXPECT_EQ(readFailure.code, PreferenceCode::ReadFailed);
        ExpectNoEffects(readFailure);
        EXPECT_EQ(storage->writes, 0);
        storage->failRead       = false;
        storage->failWrite      = true;
        const auto writeFailure = session.ApplyDetailed(&persistence);
        EXPECT_EQ(writeFailure.code, PreferenceCode::WriteFailed);
        ExpectNoEffects(writeFailure);
        EXPECT_EQ(storage->bytes, "{}");
        EXPECT_EQ(live.value, 1);
        EXPECT_EQ(Working(session), 7);
        EXPECT_EQ(session.History().GetUndoCount(), 1u);
        ASSERT_TRUE(session.History().Undo());
        EXPECT_EQ(Working(session), 1);
        ASSERT_TRUE(session.History().Redo());
        storage->failWrite = false;
        const auto retried = session.ApplyDetailed(&persistence);
        EXPECT_TRUE(retried.Succeeded()) << retried.message;
        EXPECT_EQ(live.value, 7);
        EXPECT_FALSE(session.History().CanUndo());
    }

    TEST_F(PreferencesApplyResultTest, RejectedAndThrowingValidatorsHaveNoEffectsAndDoNotConsumeHistory)
    {
        Setting live;
        int     mode  = 0;
        auto    page  = Page(live);
        page.validate = [&mode](const PreferencePage &) -> PreferenceResult {
            if (mode == 0)
                return {PreferenceCode::ValidationFailed, "plugin", "value", "rejected"};
            if (mode == 1)
                throw std::runtime_error("validator failed");
            return {};
        };
        pages->Add(std::move(page));
        PreferencesSession session;
        session.Begin(pages);
        Edit(session, 7);
        PreferencesPersistence persistence(storage);
        for (mode = 0; mode != 2; ++mode) {
            const auto result = session.ApplyDetailed(&persistence);
            EXPECT_EQ(result.code, PreferenceCode::ValidationFailed);
            ExpectNoEffects(result);
            EXPECT_EQ(live.value, 1);
            EXPECT_EQ(Working(session), 7);
            EXPECT_EQ(session.History().GetUndoCount(), 1u);
            EXPECT_EQ(storage->writes, 0);
        }
        EXPECT_TRUE(session.ApplyDetailed(&persistence).Succeeded());
        EXPECT_EQ(live.value, 7);
    }

    TEST_F(PreferencesApplyResultTest, ApplicationFailureAndThrowReportPartialEffectsAndAllowRetryOrCancel)
    {
        Setting live;
        int     mode         = 0;
        int     applications = 0;
        auto    page         = Page(live);
        page.apply           = [&mode, &applications](const PreferencePage &) -> PreferenceResult {
            ++applications;
            if (mode == 0)
                return {PreferenceCode::ApplyFailed, "plugin", {}, "application rejected"};
            if (mode == 1)
                throw std::runtime_error("application failed");
            return {};
        };
        pages->Add(std::move(page));
        PreferencesSession session;
        session.Begin(pages);
        Edit(session, 7);
        PreferencesPersistence persistence(storage);
        for (mode = 0; mode != 2; ++mode) {
            const auto result = session.ApplyDetailed(&persistence);
            EXPECT_EQ(result.code, PreferenceCode::ApplyFailed);
            EXPECT_TRUE(result.storageWritten);
            EXPECT_TRUE(result.liveWritten);
            EXPECT_TRUE(result.applicationApplied);
            EXPECT_TRUE(result.HasPartialEffects());
            EXPECT_EQ(live.value, 7);
            EXPECT_EQ(Working(session), 7);
            EXPECT_EQ(session.History().GetUndoCount(), 1u);
        }
        EXPECT_TRUE(session.ApplyDetailed(&persistence).Succeeded());
        EXPECT_EQ(applications, 3);
        EXPECT_FALSE(session.History().CanUndo());
        Edit(session, 9);
        mode = 0;
        ASSERT_FALSE(session.ApplyDetailed(&persistence).Succeeded());
        const auto persisted = storage->bytes;
        session.End();
        EXPECT_EQ(live.value, 9);
        EXPECT_EQ(storage->bytes, persisted);
        EXPECT_FALSE(session.History().CanUndo());
    }

    TEST_F(PreferencesApplyResultTest, SetterRejectionReportsEnteredLiveStageAndPreservesRetryState)
    {
        Setting live;
        pages->Add(Page(live));
        PreferencesSession session;
        session.Begin(pages);
        Edit(session, 7);
        live.rejectWrites = true;
        PreferencesPersistence persistence(storage);
        const auto             result = session.ApplyDetailed(&persistence);
        EXPECT_EQ(result.code, PreferenceCode::ApplyFailed);
        EXPECT_TRUE(result.storageWritten);
        EXPECT_TRUE(result.liveWritten);
        EXPECT_FALSE(result.applicationApplied);
        EXPECT_TRUE(result.HasPartialEffects());
        EXPECT_EQ(live.value, 1);
        EXPECT_EQ(Working(session), 7);
        EXPECT_EQ(session.History().GetUndoCount(), 1u);
        live.rejectWrites = false;
        EXPECT_TRUE(session.ApplyDetailed(&persistence).Succeeded());
        EXPECT_EQ(live.value, 7);
    }

    TEST_F(PreferencesApplyResultTest, RegistryAbaAndCallerCancellationStopBeforeOldCandidatesCanBePublished)
    {
        Setting live;
        bool    replace = false;
        auto    page    = Page(live);
        page.validate   = [this, &live, &replace](const PreferencePage &) {
            if (replace) {
                pages->Clear();
                pages->Add(Page(live));
            }
            return PreferenceResult{};
        };
        pages->Add(std::move(page));
        PreferencesSession session;
        session.Begin(pages);
        Edit(session, 7);
        PreferencesPersistence persistence(storage);
        const auto             cancelled = session.ApplyDetailed(&persistence, []() { return false; });
        EXPECT_EQ(cancelled.code, PreferenceCode::Conflict);
        ExpectNoEffects(cancelled);
        replace             = true;
        const auto replaced = session.ApplyDetailed(&persistence);
        EXPECT_EQ(replaced.code, PreferenceCode::Conflict);
        ExpectNoEffects(replaced);
        EXPECT_FALSE(session.MatchesRegistry());
        EXPECT_EQ(live.value, 1);
        EXPECT_EQ(Working(session), 7);
        EXPECT_EQ(session.History().GetUndoCount(), 1u);
        EXPECT_EQ(storage->writes, 0);
    }

    TEST_F(PreferencesApplyResultTest, GuardEndingTheSessionCannotAuthorizeAnOldApply)
    {
        Setting live;
        pages->Add(Page(live));
        PreferencesSession session;
        session.Begin(pages);
        Edit(session, 7);
        PreferencesPersistence persistence(storage);
        const auto             result = session.ApplyDetailed(&persistence, [&session]() {
            session.End();
            return true;
        });
        EXPECT_EQ(result.code, PreferenceCode::Conflict);
        ExpectNoEffects(result);
        EXPECT_FALSE(session.IsOpen());
        EXPECT_EQ(session.GetPageCount(), 0u);
        EXPECT_FALSE(session.History().CanUndo());
        EXPECT_EQ(storage->reads, 0);
        EXPECT_EQ(storage->writes, 0);
        EXPECT_EQ(live.value, 1);
    }

    TEST_F(PreferencesApplyResultTest, ThrowingGuardReturnsConflictAndLeavesWorkingHistoryAvailableForRetry)
    {
        Setting live;
        pages->Add(Page(live));
        PreferencesSession session;
        session.Begin(pages);
        Edit(session, 7);
        PreferencesPersistence persistence(storage);
        PreferenceResult       result;
        ASSERT_NO_THROW(result = session.ApplyDetailed(&persistence, []() -> bool { throw std::runtime_error("guard failed"); }));
        EXPECT_EQ(result.code, PreferenceCode::Conflict);
        ExpectNoEffects(result);
        EXPECT_TRUE(session.IsOpen());
        EXPECT_EQ(Working(session), 7);
        EXPECT_EQ(session.History().GetUndoCount(), 1u);
        EXPECT_EQ(storage->reads, 0);
        EXPECT_EQ(storage->writes, 0);
        EXPECT_EQ(live.value, 1);
        ASSERT_TRUE(session.ApplyDetailed(&persistence).Succeeded());
        EXPECT_EQ(live.value, 7);
        EXPECT_FALSE(session.History().CanUndo());
    }

    TEST_F(PreferencesApplyResultTest, StorageReadEditingWorkingValuesConflictsBeforeWritingAndCanRetryTheNewValue)
    {
        Setting live;
        pages->Add(Page(live));
        PreferencesSession session;
        session.Begin(pages);
        Edit(session, 7);
        storage->onRead = [&session]() { Edit(session, 9); };
        PreferencesPersistence persistence(storage);
        const auto             result = session.ApplyDetailed(&persistence);
        EXPECT_EQ(result.code, PreferenceCode::Conflict);
        ExpectNoEffects(result);
        EXPECT_EQ(storage->writes, 0);
        EXPECT_EQ(storage->bytes, "{}");
        EXPECT_EQ(live.value, 1);
        EXPECT_EQ(Working(session), 9);
        EXPECT_EQ(session.History().GetUndoCount(), 2u);
        storage->onRead = {};
        ASSERT_TRUE(session.ApplyDetailed(&persistence).Succeeded());
        EXPECT_EQ(live.value, 9);
        EXPECT_EQ(StoredValue(), 9);
        EXPECT_FALSE(session.History().CanUndo());
    }

    TEST_F(PreferencesApplyResultTest, ApplicationHookEditingWorkingValuesReportsPartialConflictAndKeepsBothEdits)
    {
        Setting            live;
        PreferencesSession session;
        bool               editDuringApply = true;
        auto               page            = Page(live);
        page.apply                         = [&session, &editDuringApply](const PreferencePage &) {
            if (editDuringApply) {
                editDuringApply = false;
                Edit(session, 9);
            }
            return PreferenceResult{};
        };
        pages->Add(std::move(page));
        session.Begin(pages);
        Edit(session, 7);
        PreferencesPersistence persistence(storage);
        const auto             result = session.ApplyDetailed(&persistence);
        EXPECT_EQ(result.code, PreferenceCode::Conflict);
        EXPECT_TRUE(result.storageWritten);
        EXPECT_TRUE(result.liveWritten);
        EXPECT_TRUE(result.applicationApplied);
        EXPECT_TRUE(result.HasPartialEffects());
        EXPECT_EQ(live.value, 7);
        EXPECT_EQ(StoredValue(), 7);
        EXPECT_EQ(Working(session), 9);
        EXPECT_EQ(session.History().GetUndoCount(), 2u);
        ASSERT_TRUE(session.ApplyDetailed(&persistence).Succeeded());
        EXPECT_EQ(live.value, 9);
        EXPECT_EQ(StoredValue(), 9);
        EXPECT_FALSE(session.History().CanUndo());
    }

    TEST_F(PreferencesApplyResultTest, PendingTransactionEditsConflictEvenWhenUndoStackCountsDoNotChange)
    {
        Setting live;
        pages->Add(Page(live));
        PreferencesSession session;
        session.Begin(pages);
        Edit(session, 7);
        session.History().BeginTransaction("callback settings");
        const auto undoCount = session.History().GetUndoCount();
        storage->onRead      = [&session]() { Edit(session, 9); };
        PreferencesPersistence persistence(storage);
        const auto             result = session.ApplyDetailed(&persistence);
        EXPECT_EQ(result.code, PreferenceCode::Conflict);
        ExpectNoEffects(result);
        EXPECT_EQ(storage->writes, 0);
        EXPECT_EQ(Working(session), 9);
        EXPECT_EQ(live.value, 1);
        EXPECT_EQ(session.History().GetUndoCount(), undoCount);
        session.History().EndTransaction();
        EXPECT_EQ(session.History().GetUndoCount(), undoCount + 1u);
        storage->onRead = {};
        ASSERT_TRUE(session.ApplyDetailed(&persistence).Succeeded());
        EXPECT_EQ(live.value, 9);
        EXPECT_EQ(StoredValue(), 9);
        EXPECT_FALSE(session.History().CanUndo());
    }

    TEST_F(PreferencesApplyResultTest, DirectHelpersRejectEmptyValuesAndConvertGuardExceptionsWithoutLosingEffects)
    {
        Setting live;
        pages->Add(Page(live));
        std::vector<PreferenceValue> values;
        ASSERT_TRUE(CapturePreferenceValues(pages->GetPages(), values).Succeeded());
        ASSERT_EQ(values.size(), 1u);
        values.front().value = Any{};
        const auto empty     = PublishPreferenceValues(*pages, values);
        EXPECT_EQ(empty.code, PreferenceCode::InvalidData);
        ExpectNoEffects(empty);
        EXPECT_EQ(live.value, 1);
        ASSERT_TRUE(CapturePreferenceValues(pages->GetPages(), values).Succeeded());
        static_cast<Setting *>(values.front().value.Data())->value = 7;
        const auto invalidGuard = ValidatePreferenceValues(*pages, values, []() -> bool { throw std::runtime_error("validation guard failed"); });
        EXPECT_EQ(invalidGuard.code, PreferenceCode::Conflict);
        ExpectNoEffects(invalidGuard);
        bool enteredSetter   = false;
        live.onSet           = [&enteredSetter]() { enteredSetter = true; };
        const auto published = PublishPreferenceValues(*pages, values, [&enteredSetter]() {
            if (enteredSetter)
                throw std::runtime_error("publication guard failed");
            return true;
        });
        EXPECT_EQ(published.code, PreferenceCode::Conflict);
        EXPECT_TRUE(published.liveWritten);
        EXPECT_FALSE(published.storageWritten);
        EXPECT_FALSE(published.applicationApplied);
        EXPECT_TRUE(published.HasPartialEffects());
        EXPECT_TRUE(enteredSetter);
        EXPECT_EQ(live.value, 7);
    }

    TEST_F(PreferencesApplyResultTest, NonConstAccessorPropertiesApplyAndRoundTripThroughThePortableCodec)
    {
        AccessorSettings live;
        const auto       expectedPath = live.path;
        pages->Add({"accessor", "Accessor", &live, GetTypeNode(TypeInfo<AccessorSettings>::RegisteredId())});
        PreferencesSession session;
        session.Begin(pages);
        const auto *page = session.GetPage(0);
        ASSERT_NE(page, nullptr);
        PropertyDescriptor descriptor{page->object, &page->type->members.at("value"), "value", ""};
        session.History().Execute(descriptor.MakeEditCommand(Any(int32_t{7})));
        PreferencesPersistence persistence(storage);
        const auto             applied = session.ApplyDetailed(&persistence);
        ASSERT_TRUE(applied.Succeeded()) << applied.message;
        EXPECT_TRUE(applied.storageWritten);
        EXPECT_TRUE(applied.liveWritten);
        EXPECT_EQ(live.value, 7);
        EXPECT_EQ(live.path, expectedPath);
        EXPECT_FALSE(session.History().CanUndo());
        session.End();
        live.value        = 99;
        live.path         = "different provider value";
        const auto loaded = persistence.Load(*pages);
        EXPECT_TRUE(loaded.Succeeded()) << loaded.message;
        EXPECT_EQ(live.value, 7);
        EXPECT_EQ(live.path, expectedPath);
    }

    TEST_F(PreferencesApplyResultTest, ApplicationReentryCannotClearTheReplacementSessionsHistory)
    {
        Setting            live;
        PreferencesSession session;
        bool               reenter = true;
        auto               page    = Page(live);
        page.apply                 = [this, &session, &reenter](const PreferencePage &) {
            if (reenter) {
                reenter = false;
                session.Begin(pages);
                Edit(session, 99);
            }
            return PreferenceResult{};
        };
        pages->Add(std::move(page));
        session.Begin(pages);
        Edit(session, 7);
        PreferencesPersistence persistence(storage);
        const auto             result = session.ApplyDetailed(&persistence);
        EXPECT_EQ(result.code, PreferenceCode::Conflict);
        EXPECT_TRUE(result.storageWritten);
        EXPECT_TRUE(result.liveWritten);
        EXPECT_TRUE(result.applicationApplied);
        EXPECT_TRUE(result.HasPartialEffects());
        EXPECT_EQ(live.value, 7);
        EXPECT_EQ(Working(session), 99);
        EXPECT_EQ(session.History().GetUndoCount(), 1u);
        ASSERT_TRUE(session.History().Undo());
        EXPECT_EQ(Working(session), 7);
    }

    TEST_F(PreferencesApplyResultTest, DeletingSessionOrPersistenceOwnersStopsSafelyWithoutConsumingSurvivingHistory)
    {
        Setting live;
        auto    session = std::make_unique<PreferencesSession>();
        auto    page    = Page(live);
        page.apply      = [&session](const PreferencePage &) {
            session.reset();
            return PreferenceResult{};
        };
        pages->Add(std::move(page));
        session->Begin(pages);
        Edit(*session, 7);
        auto       persistence    = std::make_unique<PreferencesPersistence>(storage);
        const auto deletedSession = session->ApplyDetailed(persistence.get());
        EXPECT_EQ(session, nullptr);
        EXPECT_EQ(deletedSession.code, PreferenceCode::Conflict);
        EXPECT_TRUE(deletedSession.storageWritten);
        EXPECT_TRUE(deletedSession.liveWritten);
        EXPECT_TRUE(deletedSession.applicationApplied);

        pages->Clear();
        pages->Add(Page(live));
        session = std::make_unique<PreferencesSession>();
        session->Begin(pages);
        Edit(*session, 9);
        storage->onWrite              = [&persistence]() { persistence.reset(); };
        const auto deletedPersistence = session->ApplyDetailed(persistence.get());
        EXPECT_EQ(persistence, nullptr);
        EXPECT_EQ(deletedPersistence.code, PreferenceCode::Conflict);
        EXPECT_TRUE(deletedPersistence.storageWritten);
        EXPECT_FALSE(deletedPersistence.liveWritten);
        EXPECT_EQ(live.value, 7);
        EXPECT_EQ(Working(*session), 9);
        EXPECT_EQ(session->History().GetUndoCount(), 1u);
    }

} // namespace sky::editor::preferences_apply_result_test

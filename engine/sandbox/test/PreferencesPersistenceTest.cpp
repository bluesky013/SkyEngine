#include "ReflectionTestEnvironment.h"
#include <editor/core/preferences/PreferencesPersistence.h>
#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace sky::editor::preferences_persistence_test {

    enum class Quality : int8_t { Standard = 1, Ultra = 2 };
    struct Nested {
        bool        enabled = true;
        std::string label   = "provider nested";
    };
    struct PluginSettings {
        int32_t                  workers = 3;
        bool                     enabled = true;
        float                    scale   = 1.25f;
        Quality                  quality = Quality::Standard;
        std::string              path    = "provider path";
        std::vector<int32_t>     samples{1, 2};
        std::vector<std::string> searchPaths{"provider"};
        Nested                   nested;
    };
    struct UnsupportedSettings {
        std::unordered_map<std::string, int32_t> values{{"original", 7}};
    };

    namespace {
        class MemoryStorage : public IPreferenceStorage {
        public:
            PreferenceResult Read(std::string &document) override
            {
                ++reads;
                if (failRead)
                    return {PreferenceCode::ReadFailed, {}, {}, "injected read failure"};
                if (!exists)
                    return {PreferenceCode::NotFound};
                document = bytes;
                return {};
            }
            PreferenceResult Write(const std::string &document) override
            {
                ++writes;
                if (failWrite)
                    return {PreferenceCode::WriteFailed, {}, {}, "injected write failure"};
                bytes  = document;
                exists = true;
                return {};
            }
            bool        exists    = false;
            bool        failRead  = false;
            bool        failWrite = false;
            int         reads     = 0;
            int         writes    = 0;
            std::string bytes;
        };

        std::string Json(const rapidjson::Value &value)
        {
            rapidjson::StringBuffer                    buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            value.Accept(writer);
            return {buffer.GetString(), buffer.GetSize()};
        }

        rapidjson::Document Parse(const std::string &text)
        {
            rapidjson::Document result;
            result.Parse(text.data(), text.size());
            EXPECT_FALSE(result.HasParseError());
            return result;
        }

        void ExpectEqual(const PluginSettings &actual, const PluginSettings &expected)
        {
            EXPECT_EQ(actual.workers, expected.workers);
            EXPECT_EQ(actual.enabled, expected.enabled);
            EXPECT_FLOAT_EQ(actual.scale, expected.scale);
            EXPECT_EQ(actual.quality, expected.quality);
            EXPECT_EQ(actual.path, expected.path);
            EXPECT_EQ(actual.samples, expected.samples);
            EXPECT_EQ(actual.searchPaths, expected.searchPaths);
            EXPECT_EQ(actual.nested.enabled, expected.nested.enabled);
            EXPECT_EQ(actual.nested.label, expected.nested.label);
        }

        const auto *reflectionEnvironment = sky::editor::test::AddReflectionEnvironment([] {
            auto *context = SerializationContext::Get();
            context->Register<Quality>("PreferencesPersistenceQuality").Enum(Quality::Standard, "Standard").Enum(Quality::Ultra, "Ultra");
            context->Register<Nested>("PreferencesPersistenceNested").Member<&Nested::enabled>("enabled").Member<&Nested::label>("label");
            context->Register<PluginSettings>("PreferencesPersistencePlugin")
                .Member<&PluginSettings::workers>("workers")
                .Member<&PluginSettings::enabled>("enabled")
                .Member<&PluginSettings::scale>("scale")
                .Member<&PluginSettings::quality>("quality")
                .Member<&PluginSettings::path>("path")
                .Member<&PluginSettings::samples>("samples")
                .Member<&PluginSettings::searchPaths>("searchPaths")
                .Member<&PluginSettings::nested>("nested");
            context->Register<UnsupportedSettings>("PreferencesPersistenceUnsupported").Member<&UnsupportedSettings::values>("values");
        });

        class PreferencesPersistenceTest : public testing::Test {
        protected:
            void SetUp() override
            {
                pages = PreferencePageRegistry::Get();
                pages->Clear();
                storage = std::make_shared<MemoryStorage>();
            }
            void TearDown() override
            {
                pages->Clear();
            }
            template <class T>
            void Add(const std::string &id, T &value)
            {
                pages->Add({id, id, &value, GetTypeNode(TypeInfo<T>::RegisteredId())});
            }
            std::vector<PreferenceValue> Capture()
            {
                std::vector<PreferenceValue> values;
                const auto                   result = CapturePreferenceValues(pages->GetPages(), values);
                EXPECT_TRUE(result.Succeeded()) << result.message;
                return values;
            }
            std::string Seed(PreferencesPersistence &persistence)
            {
                const auto result = persistence.Save(Capture());
                EXPECT_TRUE(result.Succeeded()) << result.message;
                return storage->bytes;
            }
            PreferencePageRegistry        *pages = nullptr;
            std::shared_ptr<MemoryStorage> storage;
        };
    } // namespace

    TEST_F(PreferencesPersistenceTest, PluginTypedNestedAndDeepValuesRoundTripWithoutNativeModules)
    {
        PluginSettings live;
        live.workers        = 11;
        live.enabled        = false;
        live.scale          = 1.75f;
        live.quality        = Quality::Ultra;
        live.path           = std::string(180, 'x') + " /插件/\"quoted\"\npath";
        live.samples        = {-7, 0, 42};
        live.searchPaths    = {"assets/模型", std::string(100, 'q'), ""};
        live.nested         = {false, std::string(90, 'n')};
        const auto expected = live;
        Add("plugin.render", live);
        PreferencesPersistence persistence(storage);
        const auto             saved = persistence.Save(Capture());
        ASSERT_TRUE(saved.Succeeded()) << saved.message;
        EXPECT_TRUE(saved.storageWritten);
        EXPECT_FALSE(saved.liveWritten);
        auto document = Parse(storage->bytes);
        ASSERT_TRUE(document.HasMember("plugin.render"));
        EXPECT_EQ(document["plugin.render"]["classId"].GetString(), TypeInfo<PluginSettings>::RegisteredId().ToString());
        live              = {};
        const auto loaded = persistence.Load(*pages);
        ASSERT_TRUE(loaded.Succeeded()) << loaded.message;
        ExpectEqual(live, expected);
    }

    TEST_F(PreferencesPersistenceTest, KnownArraysReplaceProviderContentsAndCanBeSavedEmpty)
    {
        PluginSettings live;
        live.samples     = {7, 8};
        live.searchPaths = {"saved"};
        Add("plugin", live);
        PreferencesPersistence persistence(storage);
        Seed(persistence);
        live.samples     = {99, 98, 97};
        live.searchPaths = {"provider", "tail"};
        ASSERT_TRUE(persistence.Load(*pages).Succeeded());
        EXPECT_EQ(live.samples, (std::vector<int32_t>{7, 8}));
        EXPECT_EQ(live.searchPaths, (std::vector<std::string>{"saved"}));
        live.samples.clear();
        live.searchPaths.clear();
        ASSERT_TRUE(persistence.Save(Capture()).Succeeded());
        live.samples     = {1};
        live.searchPaths = {"not empty"};
        ASSERT_TRUE(persistence.Load(*pages).Succeeded());
        EXPECT_TRUE(live.samples.empty());
        EXPECT_TRUE(live.searchPaths.empty());
    }

    TEST_F(PreferencesPersistenceTest, UnknownPagesEnvelopeKeysAndNestedMembersSurviveKnownEdits)
    {
        PluginSettings live;
        Add("plugin", live);
        PreferencesPersistence persistence(storage);
        auto                   document  = Parse(Seed(persistence));
        auto                  &allocator = document.GetAllocator();
        rapidjson::Document    opaque;
        opaque.Parse(R"({"classId":"not-installed","elements":{"future":[1,{"opaque":true}]},"version":8})");
        document.AddMember("absent.plugin", rapidjson::Value(opaque, allocator), allocator);
        document.AddMember("future-root-key", rapidjson::Value("keep root", allocator), allocator);
        document["plugin"].AddMember("future-envelope", 17, allocator);
        document["plugin"]["elements"].AddMember("future-member", rapidjson::Value("keep member", allocator), allocator);
        auto &nested = document["plugin"]["elements"]["nested"];
        nested.AddMember("future-nested-envelope", true, allocator);
        nested["elements"].AddMember("future-nested-member", rapidjson::Value(opaque, allocator), allocator);
        const auto expectedOpaque = Json(document["absent.plugin"]);
        const auto expectedNested = Json(nested["elements"]["future-nested-member"]);
        storage->bytes            = Json(document);
        ASSERT_TRUE(persistence.Load(*pages).Succeeded());
        live.path         = "changed known field";
        live.nested.label = "changed nested field";
        ASSERT_TRUE(persistence.Save(Capture()).Succeeded());
        auto merged = Parse(storage->bytes);
        EXPECT_EQ(Json(merged["absent.plugin"]), expectedOpaque);
        EXPECT_STREQ(merged["future-root-key"].GetString(), "keep root");
        EXPECT_EQ(merged["plugin"]["future-envelope"].GetInt(), 17);
        EXPECT_STREQ(merged["plugin"]["elements"]["future-member"].GetString(), "keep member");
        const auto &mergedNested = merged["plugin"]["elements"]["nested"];
        EXPECT_TRUE(mergedNested["future-nested-envelope"].GetBool());
        EXPECT_EQ(Json(mergedNested["elements"]["future-nested-member"]), expectedNested);
        EXPECT_STREQ(merged["plugin"]["elements"]["path"].GetString(), "changed known field");
    }

    TEST_F(PreferencesPersistenceTest, MissingPagesAndMembersPreserveCurrentProviderDefaults)
    {
        PluginSettings live;
        live.path    = "provider-specific default";
        live.samples = {55};
        Add("plugin", live);
        PreferencesPersistence persistence(storage);
        storage->exists     = true;
        storage->bytes      = "{}";
        const auto defaults = live;
        ASSERT_TRUE(persistence.Load(*pages).Succeeded());
        ExpectEqual(live, defaults);
        auto  document = Parse(Seed(persistence));
        auto &elements = document["plugin"]["elements"];
        elements.RemoveMember("path");
        elements.RemoveMember("samples");
        elements["workers"].SetInt(9);
        elements["nested"]["elements"].RemoveMember("label");
        storage->bytes    = Json(document);
        live.path         = "new provider default";
        live.samples      = {66, 77};
        live.nested.label = "new nested default";
        ASSERT_TRUE(persistence.Load(*pages).Succeeded());
        EXPECT_EQ(live.workers, 9);
        EXPECT_EQ(live.path, "new provider default");
        EXPECT_EQ(live.samples, (std::vector<int32_t>{66, 77}));
        EXPECT_EQ(live.nested.label, "new nested default");
    }

    TEST_F(PreferencesPersistenceTest, MalformedTypedOutOfRangeAndDuplicateJsonNeverPartiallyPublish)
    {
        PluginSettings first;
        PluginSettings second;
        Add("alpha", first);
        Add("zeta", second);
        PreferencesPersistence                                        persistence(storage);
        const auto                                                    seed           = Seed(persistence);
        const auto                                                    expectedFirst  = first;
        const auto                                                    expectedSecond = second;
        std::vector<std::string>                                      invalid{"{\"alpha\":", "[]"};
        const std::vector<std::function<void(rapidjson::Document &)>> corrupt = {
            [](auto &document) { document["zeta"]["elements"]["workers"].SetString("wrong"); },
            [](auto &document) { document["zeta"]["elements"]["workers"].SetInt64(2147483648LL); },
            [](auto &document) { document["zeta"]["elements"]["quality"].SetInt(1000); },
            [](auto &document) { document["zeta"]["elements"]["samples"][0].SetString("wrong element"); },
            [](auto &document) { document["zeta"]["elements"]["nested"]["classId"].SetString("wrong class"); },
            [](auto &document) { document["zeta"]["elements"].AddMember("workers", 9, document.GetAllocator()); }};
        for (const auto &change : corrupt) {
            auto document = Parse(seed);
            document["alpha"]["elements"]["workers"].SetInt(37);
            change(document);
            invalid.push_back(Json(document));
        }
        for (const auto &bad : invalid) {
            SCOPED_TRACE(bad);
            storage->bytes    = bad;
            const auto result = persistence.Load(*pages);
            EXPECT_FALSE(result.Succeeded());
            EXPECT_FALSE(result.liveWritten);
            EXPECT_FALSE(result.HasPartialEffects());
            ExpectEqual(first, expectedFirst);
            ExpectEqual(second, expectedSecond);
        }
    }

    TEST_F(PreferencesPersistenceTest, ReadFailureAndNotFoundLeaveProviderValuesUntouched)
    {
        PluginSettings live;
        Add("plugin", live);
        const auto             expected = live;
        PreferencesPersistence persistence(storage);
        storage->failRead = true;
        EXPECT_EQ(persistence.Load(*pages).code, PreferenceCode::ReadFailed);
        storage->failRead = false;
        EXPECT_EQ(persistence.Load(*pages).code, PreferenceCode::NotFound);
        EXPECT_EQ(storage->writes, 0);
        ExpectEqual(live, expected);
    }

    TEST_F(PreferencesPersistenceTest, EveryCandidateIsValidatedBeforeAnyLiveValueOrApplicationHook)
    {
        PluginSettings first;
        PluginSettings second;
        Add("alpha", first);
        int            applications = 0;
        PreferencePage page{"zeta", "zeta", &second, GetTypeNode(TypeInfo<PluginSettings>::RegisteredId())};
        page.validate = [&second](const PreferencePage &candidate) {
            EXPECT_NE(candidate.object, &second);
            const auto &value = *static_cast<const PluginSettings *>(candidate.object);
            return value.workers > 10 ? PreferenceResult{PreferenceCode::ValidationFailed, candidate.id, "workers", "too many workers"}
                                      : PreferenceResult{};
        };
        page.apply = [&applications](const PreferencePage &) {
            ++applications;
            return PreferenceResult{};
        };
        pages->Add(std::move(page));
        PreferencesPersistence persistence(storage);
        auto                   document = Parse(Seed(persistence));
        document["alpha"]["elements"]["workers"].SetInt(7);
        document["zeta"]["elements"]["workers"].SetInt(99);
        storage->bytes    = Json(document);
        const auto result = persistence.Load(*pages);
        EXPECT_EQ(result.code, PreferenceCode::ValidationFailed);
        EXPECT_EQ(result.pageId, "zeta");
        EXPECT_FALSE(result.HasPartialEffects());
        EXPECT_EQ(first.workers, 3);
        EXPECT_EQ(second.workers, 3);
        EXPECT_EQ(applications, 0);
    }

    TEST_F(PreferencesPersistenceTest, UnsupportedReflectedContainersFailWithoutWritingOrChangingLiveValues)
    {
        UnsupportedSettings live;
        Add("unsupported", live);
        PreferencesPersistence persistence(storage);
        const auto             result = persistence.Save(Capture());
        EXPECT_EQ(result.code, PreferenceCode::UnsupportedType);
        EXPECT_FALSE(result.HasPartialEffects());
        EXPECT_EQ(storage->writes, 0);
        EXPECT_EQ(live.values.at("original"), 7);
    }

    TEST_F(PreferencesPersistenceTest, DuplicatePageRegistrationIsRejectedBeforePersistenceOrLiveEffects)
    {
        PluginSettings first;
        PluginSettings second;
        Add("duplicate", first);
        Add("duplicate", second);
        std::vector<PreferenceValue> values;
        EXPECT_FALSE(CapturePreferenceValues(pages->GetPages(), values).Succeeded());
        PreferencesPersistence persistence(storage);
        storage->exists   = true;
        storage->bytes    = "{}";
        const auto result = persistence.Load(*pages);
        EXPECT_FALSE(result.Succeeded());
        EXPECT_FALSE(result.HasPartialEffects());
        EXPECT_EQ(storage->writes, 0);
        EXPECT_EQ(first.workers, 3);
        EXPECT_EQ(second.workers, 3);
    }

    TEST_F(PreferencesPersistenceTest, FailedWritePreservesTheOriginalDocumentAndCanBeRetried)
    {
        PluginSettings live;
        Add("plugin", live);
        PreferencesPersistence persistence(storage);
        const auto             original = Seed(persistence);
        live.path                       = "retry value";
        const auto values               = Capture();
        storage->failWrite              = true;
        const auto failed               = persistence.Save(values);
        EXPECT_EQ(failed.code, PreferenceCode::WriteFailed);
        EXPECT_FALSE(failed.HasPartialEffects());
        EXPECT_EQ(storage->bytes, original);
        storage->failWrite = false;
        const auto retried = persistence.Save(values);
        EXPECT_TRUE(retried.Succeeded()) << retried.message;
        EXPECT_TRUE(retried.storageWritten);
        EXPECT_FALSE(retried.liveWritten);
        EXPECT_NE(storage->bytes, original);
    }

} // namespace sky::editor::preferences_persistence_test

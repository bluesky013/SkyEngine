#include <core/environment/Once.h>
#include <core/util/Uuid.h>
#include <editor/core/preferences/FilePreferenceStorage.h>
#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

namespace sky::editor::file_preference_storage_test {
    namespace fs = std::filesystem;

    struct Settings {
        int32_t     value = 3;
        std::string label = "provider";
    };

    namespace {
        class FilePreferenceStorageTest : public testing::Test {
        protected:
            void SetUp() override
            {
                root = fs::temp_directory_path() / ("sky-preference-storage-" + Uuid::Create().ToString());
                fs::create_directories(root);
            }
            void TearDown() override
            {
                std::error_code ignored;
                fs::permissions(root, fs::perms::owner_all, fs::perm_options::add, ignored);
                fs::remove_all(root, ignored);
            }
            static std::string Bytes(const fs::path &path)
            {
                std::ifstream file(path, std::ios::binary);
                return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
            }
            static void Put(const fs::path &path, const std::string &bytes)
            {
                std::ofstream file(path, std::ios::binary | std::ios::trunc);
                ASSERT_TRUE(file.is_open());
                file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
                ASSERT_TRUE(file.good());
            }
            static void ExpectNoEffects(const PreferenceResult &result)
            {
                EXPECT_FALSE(result.storageWritten);
                EXPECT_FALSE(result.liveWritten);
                EXPECT_FALSE(result.applicationApplied);
            }
            std::size_t EntryCount(const fs::path &directory) const
            {
                return static_cast<std::size_t>(std::distance(fs::directory_iterator(directory), fs::directory_iterator{}));
            }
            fs::path root;
        };
    } // namespace

    TEST_F(FilePreferenceStorageTest, MissingFileIsNotFoundAndReadFailuresLeaveOutputIntact)
    {
        std::string           document = "caller bytes";
        FilePreferenceStorage missing(root / "missing" / "preferences.json");
        EXPECT_EQ(missing.Read(document).code, PreferenceCode::NotFound);
        EXPECT_EQ(document, "caller bytes");
        FilePreferenceStorage directory(root);
        const auto            failed = directory.Read(document);
        EXPECT_EQ(failed.code, PreferenceCode::ReadFailed);
        ExpectNoEffects(failed);
        EXPECT_EQ(document, "caller bytes");
        EXPECT_EQ(EntryCount(root), 0u);
    }

    TEST_F(FilePreferenceStorageTest, CreatesParentsAndPreservesBinaryBytesAcrossFreshInstances)
    {
        const auto  path = root / fs::path(u8"设置目录") / fs::path(u8"偏好.json");
        std::string expected(85000, 'x');
        expected.replace(42, 3, std::string("\0\r\n", 3));
        expected += " UTF-8: 插件";
        {
            FilePreferenceStorage storage(path);
            EXPECT_EQ(storage.GetPath(), path);
            const auto written = storage.Write(expected);
            ASSERT_TRUE(written.Succeeded()) << written.message;
            EXPECT_TRUE(written.storageWritten);
        }
        FilePreferenceStorage reopened(path);
        std::string           actual = "old output";
        ASSERT_TRUE(reopened.Read(actual).Succeeded());
        EXPECT_EQ(actual, expected);
        EXPECT_EQ(EntryCount(path.parent_path()), 1u);
    }

    TEST_F(FilePreferenceStorageTest, ReplacesExistingBytesWithoutAppendingAndSupportsAnEmptyFile)
    {
        const auto        path = root / "preferences.json";
        const std::string oldBytes(32000, 'o');
        Put(path, oldBytes);
        FilePreferenceStorage storage(path);
        ASSERT_TRUE(storage.Write("replacement").Succeeded());
        EXPECT_EQ(Bytes(path), "replacement");
        ASSERT_TRUE(storage.Write("").Succeeded());
        EXPECT_TRUE(Bytes(path).empty());
        std::string actual = "nonempty";
        ASSERT_TRUE(storage.Read(actual).Succeeded());
        EXPECT_TRUE(actual.empty());
        EXPECT_EQ(EntryCount(root), 1u);
    }

    TEST_F(FilePreferenceStorageTest, InvalidAndBlockedPathsReturnTypedFailuresWithoutChangingOtherFiles)
    {
        const auto blocker = root / "existing-file";
        Put(blocker, "keep these bytes");
        const fs::path invalidPaths[] = {fs::path{}, blocker / "preferences.json", root / ".", fs::path(std::string("bad\0path", 8))};
        for (const auto &path : invalidPaths) {
            FilePreferenceStorage storage(path);
            std::string           output = "untouched";
            EXPECT_EQ(storage.Read(output).code, PreferenceCode::ReadFailed);
            EXPECT_EQ(output, "untouched");
            const auto failed = storage.Write("replacement");
            EXPECT_EQ(failed.code, PreferenceCode::WriteFailed);
            ExpectNoEffects(failed);
            EXPECT_EQ(Bytes(blocker), "keep these bytes");
        }
        EXPECT_EQ(EntryCount(root), 1u);
    }

    TEST_F(FilePreferenceStorageTest, FailedReplacementCleansItsTemporaryFileAndKeepsTheOldDestination)
    {
        const auto destination = root / "preferences.json";
        fs::create_directory(destination);
        Put(destination / "keep", "old destination bytes");
        FilePreferenceStorage storage(destination);
        const auto            failed = storage.Write("replacement");
        EXPECT_EQ(failed.code, PreferenceCode::WriteFailed);
        ExpectNoEffects(failed);
        EXPECT_TRUE(fs::is_directory(destination));
        EXPECT_EQ(Bytes(destination / "keep"), "old destination bytes");
        EXPECT_EQ(EntryCount(root), 1u);
    }

    TEST_F(FilePreferenceStorageTest, ReadOnlyDestinationFailurePreservesPriorBytes)
    {
        const auto path = root / "preferences.json";
        Put(path, "previous complete file");
        std::error_code error;
#if defined(_WIN32)
        fs::permissions(path, fs::perms::owner_read, fs::perm_options::replace, error);
#else
        fs::permissions(root, fs::perms::owner_read | fs::perms::owner_exec, fs::perm_options::replace, error);
#endif
        if (error)
            GTEST_SKIP() << "Cannot create read-only fixture: " << error.message();
        FilePreferenceStorage storage(path);
        const auto            failed = storage.Write("replacement");
#if defined(_WIN32)
        fs::permissions(path, fs::perms::owner_all, fs::perm_options::replace, error);
#else
        fs::permissions(root, fs::perms::owner_all, fs::perm_options::replace, error);
#endif
        ASSERT_FALSE(error);
        if (failed.Succeeded())
            GTEST_SKIP() << "Execution identity/filesystem bypasses the read-only fixture";
        EXPECT_EQ(failed.code, PreferenceCode::WriteFailed);
        ExpectNoEffects(failed);
        EXPECT_EQ(Bytes(path), "previous complete file");
        EXPECT_EQ(EntryCount(root), 1u);
    }

    TEST_F(FilePreferenceStorageTest, PersistenceRestartsFromTheRealFileAndRejectsBadExistingJsonWithoutReplacement)
    {
        if (Once::Get()->Run("FilePreferenceStorageTest.Settings")) {
            SerializationContext::Get()
                ->Register<Settings>("FilePreferenceStorageSettings")
                .Member<&Settings::value>("value")
                .Member<&Settings::label>("label");
        }
        auto *pages = PreferencePageRegistry::Get();
        pages->Clear();
        struct ResetPages {
            ~ResetPages()
            {
                PreferencePageRegistry::Get()->Clear();
            }
        } reset;
        Settings   live{17, "saved 插件"};
        const auto path = root / "preferences.json";
        pages->Add({"plugin", "Plugin", &live, GetTypeNode(TypeInfo<Settings>::RegisteredId())});
        {
            PreferencesPersistence       persistence(std::make_shared<FilePreferenceStorage>(path));
            std::vector<PreferenceValue> values;
            ASSERT_TRUE(CapturePreferenceValues(pages->GetPages(), values).Succeeded());
            ASSERT_TRUE(persistence.Save(values).Succeeded());
        }
        pages->Clear();
        Settings restored;
        pages->Add({"plugin", "Plugin", &restored, GetTypeNode(TypeInfo<Settings>::RegisteredId())});
        PreferencesPersistence restarted(std::make_shared<FilePreferenceStorage>(path));
        ASSERT_TRUE(restarted.Load(*pages).Succeeded());
        EXPECT_EQ(restored.value, 17);
        EXPECT_EQ(restored.label, "saved 插件");
        Put(path, "{ malformed existing document");
        std::vector<PreferenceValue> values;
        ASSERT_TRUE(CapturePreferenceValues(pages->GetPages(), values).Succeeded());
        const auto failed = restarted.Save(values);
        EXPECT_EQ(failed.code, PreferenceCode::InvalidJson);
        ExpectNoEffects(failed);
        EXPECT_EQ(Bytes(path), "{ malformed existing document");
        EXPECT_EQ(EntryCount(root), 1u);
    }
} // namespace sky::editor::file_preference_storage_test

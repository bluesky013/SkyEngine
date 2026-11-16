//
// Created on 2026/11/10.
//

#include <editor/core/document/SourceDocumentStorage.h>

#include <core/file/FileSystem.h>
#include <core/file/FileUtil.h>
#include <framework/asset/AssetDataBase.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>

namespace sky::editor {
    namespace {
        namespace fs = std::filesystem;

        class SourceDocumentStorageTest : public testing::Test {
        protected:
            void SetUp() override
            {
                previousWorkspace = database->GetWorkSpaceFs();
                std::error_code error;
                root = fs::temp_directory_path(error) / ("sky-source-storage-" + Uuid::Create().ToString());
                ASSERT_FALSE(error) << error.message();
                ASSERT_TRUE(fs::create_directories(root, error));
                ASSERT_FALSE(error) << error.message();
                target = root / "document.source-storage-test";
                Put(target, "original source");
                ASSERT_FALSE(HasFatalFailure());
                database->Reset();
                database->SetWorkSpaceFs(new NativeFileSystem(FilePath(root)));
                source = database->RegisterAsset("document.source-storage-test", false);
                ASSERT_NE(source, nullptr);
                storage = MakeAssetSourceStorage(source->uuid, MAX_BYTES);
            }

            void TearDown() override
            {
                database->Reset();
                database->SetWorkSpaceFs(previousWorkspace);
                if (previousWorkspace)
                    EXPECT_TRUE(database->RebuildCacheFromScan().Succeeded());
                if (!root.empty()) {
                    std::error_code error;
                    fs::permissions(root, fs::perms::owner_all, fs::perm_options::add, error);
                    error.clear();
                    fs::remove_all(root, error);
                    EXPECT_FALSE(error) << error.message();
                }
            }

            static void Put(const fs::path &path, std::string_view text)
            {
                std::ofstream stream(path, std::ios::binary | std::ios::trunc);
                ASSERT_TRUE(stream.is_open());
                stream.write(text.data(), static_cast<std::streamsize>(text.size()));
                ASSERT_TRUE(stream.good());
            }

            static std::string Bytes(const fs::path &path)
            {
                std::ifstream stream(path, std::ios::binary);
                EXPECT_TRUE(stream.is_open());
                return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
            }

            std::set<std::string> Entries() const
            {
                std::set<std::string>  entries;
                std::error_code        error;
                fs::directory_iterator iter(root, error);
                while (!error && iter != fs::directory_iterator{}) {
                    entries.insert(iter->path().filename().string());
                    iter.increment(error);
                }
                EXPECT_FALSE(error) << error.message();
                return entries;
            }

            static constexpr size_t MAX_BYTES = 64;
            AssetDataBase          *database  = AssetDataBase::Get();
            FileSystemPtr           previousWorkspace;
            AssetSourcePtr          source;
            SourceDocumentStorage   storage;
            fs::path                root;
            fs::path                target;
        };
    } // namespace

    TEST_F(SourceDocumentStorageTest, ReadsRegisteredSourceAtTheByteLimit)
    {
        std::string resolved;
        ASSERT_TRUE(database->GetAbsoluteSourcePath(source->uuid, resolved));
        std::error_code error;
        EXPECT_TRUE(fs::equivalent(PathFromUtf8(resolved), target, error));
        ASSERT_FALSE(error) << error.message();

        std::string expected(MAX_BYTES, 'x');
        expected.replace(5, 3, std::string("\0\r\n", 3));
        Put(target, expected);
        ASSERT_FALSE(HasFatalFailure());
        std::string output = "previous output";
        const auto  result = storage.read(output);
        ASSERT_TRUE(result) << result.message;
        EXPECT_EQ(output, expected);
    }

    TEST_F(SourceDocumentStorageTest, InvalidReadsPreserveCallerOutput)
    {
        std::string output = "previous output";
        Put(target, std::string(MAX_BYTES + 1, 'x'));
        ASSERT_FALSE(HasFatalFailure());
        EXPECT_FALSE(storage.read(output));
        EXPECT_EQ(output, "previous output");

        Put(target, "");
        ASSERT_FALSE(HasFatalFailure());
        EXPECT_FALSE(storage.read(output));
        EXPECT_EQ(output, "previous output");

        std::error_code error;
        ASSERT_TRUE(fs::remove(target, error));
        ASSERT_FALSE(error) << error.message();
        EXPECT_FALSE(storage.read(output));
        EXPECT_EQ(output, "previous output");
        const auto unknown = MakeAssetSourceStorage(Uuid::Create(), MAX_BYTES);
        EXPECT_EQ(unknown.read(output).error, FileOperationError::InvalidPath);
        EXPECT_EQ(output, "previous output");
    }

    TEST_F(SourceDocumentStorageTest, AtomicWriteReplacesBytesAndLeavesNoStagingFile)
    {
        const auto foreign = root / "document.source-storage-test.foreign.tmp";
        Put(foreign, "another writer's staging file");
        ASSERT_FALSE(HasFatalFailure());
        const auto        before = Entries();
        const std::string replacement("new\0bytes", 9);
        const auto        result = storage.write(replacement);
        ASSERT_TRUE(result) << result.message;
        EXPECT_EQ(Bytes(target), replacement);
        EXPECT_EQ(Bytes(foreign), "another writer's staging file");
        EXPECT_EQ(Entries(), before);
        std::string output;
        ASSERT_TRUE(MakeAssetSourceStorage(source->uuid, MAX_BYTES).read(output));
        EXPECT_EQ(output, replacement);
    }

    TEST_F(SourceDocumentStorageTest, InvalidWritesPreserveExistingSourceAndDirectoryEntries)
    {
        const auto before = Entries();
        EXPECT_FALSE(storage.write(std::string(MAX_BYTES + 1, 'x')));
        EXPECT_EQ(Bytes(target), "original source");
        EXPECT_FALSE(storage.write(""));
        EXPECT_EQ(Bytes(target), "original source");
        const auto unknown = MakeAssetSourceStorage(Uuid::Create(), MAX_BYTES);
        EXPECT_EQ(unknown.write("replacement").error, FileOperationError::InvalidPath);
        EXPECT_EQ(Bytes(target), "original source");
        EXPECT_EQ(Entries(), before);
    }

    TEST_F(SourceDocumentStorageTest, FailedReplacementKeepsDestinationAndCleansOnlyItsStagingFile)
    {
        std::error_code error;
        ASSERT_TRUE(fs::remove(target, error));
        ASSERT_FALSE(error) << error.message();
        ASSERT_TRUE(fs::create_directory(target, error));
        ASSERT_FALSE(error) << error.message();
        Put(target / "keep", "old destination bytes");
        const auto foreign = root / "document.source-storage-test.foreign.tmp";
        Put(foreign, "another writer's staging file");
        ASSERT_FALSE(HasFatalFailure());
        const auto before = Entries();

        const auto result = storage.write("replacement");
        EXPECT_FALSE(result);
        EXPECT_NE(result.message.find("atomic replace"), std::string::npos);
        EXPECT_TRUE(fs::is_directory(target, error));
        EXPECT_FALSE(error) << error.message();
        EXPECT_EQ(Bytes(target / "keep"), "old destination bytes");
        EXPECT_EQ(Bytes(foreign), "another writer's staging file");
        EXPECT_EQ(Entries(), before);
    }

    TEST_F(SourceDocumentStorageTest, ReadOnlyFailurePreservesPriorSourceBytes)
    {
        const auto      before = Entries();
        std::error_code error;
#if defined(_WIN32)
        const auto &blocked = target;
        fs::permissions(blocked, fs::perms::owner_read, fs::perm_options::replace, error);
#else
        const auto &blocked = root;
        fs::permissions(blocked, fs::perms::owner_read | fs::perms::owner_exec, fs::perm_options::replace, error);
#endif
        if (error)
            GTEST_SKIP() << "Cannot create read-only fixture: " << error.message();
        const auto result = storage.write("replacement");
        fs::permissions(blocked, fs::perms::owner_all, fs::perm_options::add, error);
        ASSERT_FALSE(error) << error.message();
        if (result)
            GTEST_SKIP() << "Execution identity/filesystem bypasses the read-only fixture";
        EXPECT_EQ(Bytes(target), "original source");
        EXPECT_EQ(Entries(), before);
    }
} // namespace sky::editor

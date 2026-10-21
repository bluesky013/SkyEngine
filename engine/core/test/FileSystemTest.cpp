//
// Created by blues on 2024/6/16.
//

#include <chrono>
#include <codecvt>
#include <core/file/FileSystem.h>
#include <cstdio>
#include <gtest/gtest.h>
#include <iostream>
#include <set>
using namespace sky;

TEST(FileSystemTest, FileTest)
{
    FilePath path("test.txt");
    NativeFileSystem fs("./");
    {
        auto file = fs.CreateOrOpenFile(path);
        auto archive = file->WriteAsArchive();
        archive->Save(0xFFFF0000);
    }

    {
        auto file = fs.OpenFile(path);
        auto archive = file->ReadAsArchive();
        uint32_t val = 0;
        archive->Load(val);
        ASSERT_EQ(val, 0xFFFF0000);
    }

    std::remove("test.txt");
}

namespace {
    class FileScanTest : public testing::Test {
    protected:
        void SetUp() override
        {
            std::error_code error;
            const auto      temp = std::filesystem::temp_directory_path(error);
            ASSERT_FALSE(error);
            root = temp / ("sky-file-scan-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            ASSERT_TRUE(std::filesystem::create_directories(root / "nested", error));
            ASSERT_FALSE(error);
        }
        void TearDown() override
        {
            if (!root.empty()) {
                std::error_code error;
                std::filesystem::remove_all(root, error);
                EXPECT_FALSE(error);
            }
        }
        std::filesystem::path root;
    };

    TEST_F(FileScanTest, ReturnsRelativePathsForBothOverloads)
    {
        std::ofstream(root / "one.world") << "{}";
        std::ofstream(root / "nested" / "two.world") << "{}";
        std::ofstream(root / "nested" / "three.txt") << "text";
        const auto single = NativeFileSystem::FilterFiles(FilePath(root), std::string(".world"));
        ASSERT_TRUE(std::holds_alternative<std::vector<FilePath>>(single));
        std::set<std::string> paths;
        for (const auto &path : std::get<std::vector<FilePath>>(single)) {
            paths.insert(std::filesystem::path(path.GetStr()).generic_string());
        }
        EXPECT_EQ(paths, (std::set<std::string>{"one.world", "nested/two.world"}));
        const auto multiple = NativeFileSystem::FilterFiles(FilePath(root), std::vector<std::string>{".world", ".txt"});
        ASSERT_TRUE(std::holds_alternative<std::vector<FilePath>>(multiple));
        EXPECT_EQ(std::get<std::vector<FilePath>>(multiple).size(), 3u);
    }

    TEST_F(FileScanTest, EmptyDirectorySucceedsButMissingOrInvalidRootFails)
    {
        const auto empty = NativeFileSystem::FilterFiles(FilePath(root), std::string(".world"));
        ASSERT_TRUE(std::holds_alternative<std::vector<FilePath>>(empty));
        EXPECT_TRUE(std::get<std::vector<FilePath>>(empty).empty());
        const auto missing = NativeFileSystem::FilterFiles(FilePath(root / "missing"), std::string(".world"));
        ASSERT_TRUE(std::holds_alternative<FileScanError>(missing));
        EXPECT_EQ(std::get<FileScanError>(missing).path, FilePath(root / "missing"));
        EXPECT_EQ(std::get<FileScanError>(missing).code, std::errc::no_such_file_or_directory);
        std::ofstream(root / "file.world") << "{}";
        const auto invalid = NativeFileSystem::FilterFiles(FilePath(root / "file.world"), std::vector<std::string>{".world"});
        ASSERT_TRUE(std::holds_alternative<FileScanError>(invalid));
        EXPECT_EQ(std::get<FileScanError>(invalid).path, FilePath(root / "file.world"));
        EXPECT_TRUE(std::get<FileScanError>(invalid).code);
    }

} // namespace

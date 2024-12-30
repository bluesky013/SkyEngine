//
// Created by blues on 2024/6/16.
//

#include <core/file/FileSystem.h>
#include <gtest/gtest.h>
#include <iostream>
#include <codecvt>
using namespace sky;

TEST(FileSystemTest, FileTest)
{
    FilePath path("test.txt");
    NativeFileSystem fs("");
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
}
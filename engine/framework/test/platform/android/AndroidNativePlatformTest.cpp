//
// Created on 2026/10/15.
//

#include <core/platform/Platform.h>

#if defined(SKY_PLATFORM_ANDROID)
    #include "AndroidAssetArchive.h"
    #include "AndroidBundleFileSystem.h"
    #include "AndroidPlatform.h"
    #include <gtest/gtest.h>

TEST(AndroidNativePlatformTest, MissingActivityFailsWithoutInstallingCallbacks)
{
    sky::AndroidPlatform platform;
    sky::PlatformBase   &api = platform;
    EXPECT_FALSE(api.Init({}));
    EXPECT_FALSE(api.IsFrameActive());
    EXPECT_EQ(api.GetType(), sky::PlatformType::Android);
    EXPECT_TRUE(api.GetInternalPath().empty());
    bool exit = false;
    api.PollEvent(exit);
    EXPECT_TRUE(exit);
}

TEST(AndroidNativePlatformTest, ClockIsMonotonicWithNanosecondFrequency)
{
    sky::AndroidPlatform     platform;
    const sky::PlatformBase &api = platform;
    EXPECT_EQ(api.GetPerformanceFrequency(), 1000000000ULL);
    const uint64_t first  = api.GetPerformanceCounter();
    const uint64_t second = api.GetPerformanceCounter();
    EXPECT_GT(first, 0U);
    EXPECT_GE(second, first);
    EXPECT_TRUE(api.GetEnvVariable("SKY_TEST_NONEXISTENT_ENV_VARIABLE").empty());
}

TEST(AndroidNativeAssetTest, MissingAssetsFailAndDoNotExposeWritableFiles)
{
    sky::AndroidBundleFileSystem filesystem("");
    EXPECT_FALSE(filesystem.FileExist("sky-test-definitely-missing.bin"));
    EXPECT_FALSE(filesystem.OpenFile("sky-test-definitely-missing.bin"));
    EXPECT_FALSE(filesystem.CreateOrOpenFile("test.bin"));
    sky::AndroidAssetFile file("configs\\modules_game.json");
    EXPECT_EQ(file.GetPath(), "configs/modules_game.json");
}

TEST(AndroidNativeAssetTest, InvalidAssetArchiveUsesTheSameEmptyStreamForAllOperations)
{
    sky::AndroidAssetArchive archive(nullptr);
    EXPECT_FALSE(archive.IsValid());
    EXPECT_EQ(archive.Tell(), 0U);
    EXPECT_EQ(archive.Peek(), std::char_traits<char>::eof());
    char data = 0;
    EXPECT_FALSE(archive.LoadRaw(&data, 1));
}
#endif

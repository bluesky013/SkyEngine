//
// Created on 2026/10/15.
//

#include <core/platform/Platform.h>

#if defined(SKY_PLATFORM_ANDROID)
    #include <cstdlib>
    #include <gtest/gtest.h>
    #include <string_view>

struct android_app;

// The official static SDK archive includes native_app_glue, which requires this
// symbol even in a CLI test executable. This entry is a headless SDK contract
// test host, not a GameActivity lifecycle/UI integration host.
extern "C" void android_main(android_app *)
{
}

int main(int argc, char **argv)
{
    bool explicitFilter = std::getenv("GTEST_FILTER") != nullptr;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "--gtest_filter" || argument.starts_with("--gtest_filter=")) {
            explicitFilter = true;
        }
    }
    testing::InitGoogleTest(&argc, argv);
    if (!explicitFilter) {
        testing::GTEST_FLAG(filter) =
            "AndroidNativePlatformTest.*:AndroidNativeAssetTest.*:AndroidLifecycleTest.*:AndroidPointerTest.*:AndroidKeyboardTest.*:"
            "AndroidTextTest.*:AndroidAssetPathTest.*:AndroidAssetReadTest.*:FrameClockTest.*";
    }
    return RUN_ALL_TESTS();
}
#endif

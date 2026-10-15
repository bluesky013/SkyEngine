//
// UIKit creates the test scene before any engine platform fixture runs.
//

#include "IOSPlatformTestHost.h"

#include <framework/platform/IOSSceneHost.h>
#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <string_view>

#import <UIKit/UIKit.h>

namespace {

    UIWindowScene *hostScene        = nil;
    UIWindow      *activationWindow = nil;
    bool           sceneConnected   = false;
    bool           sceneActivated   = false;
    bool           testsScheduled   = false;

    void RunTestsAfterSceneActivation()
    {
        if (testsScheduled || !sceneConnected || !sceneActivated) {
            return;
        }
        testsScheduled = true;
        dispatch_async(dispatch_get_main_queue(), ^{
            const int   gtestResult = RUN_ALL_TESTS();
            const auto *tests       = ::testing::UnitTest::GetInstance();
            // A filter matching no tests (or only skipped tests) did not verify
            // the native contract and must never be reported as a success.
            const int count  = tests->successful_test_count() + tests->failed_test_count();
            const int result = gtestResult != 0 ? gtestResult : (count > 0 ? 0 : 1);
            if (count == 0) {
                std::fputs("No iOS platform tests executed; check the gtest filter\n", stderr);
            }
            std::printf("SKY_IOS_TEST_COUNT=%d\n", count);
            std::printf("SKY_IOS_TEST_EXIT=%d\n", result);
            std::fflush(stdout);
            std::fflush(stderr);
            std::exit(result);
        });
    }

} // namespace

namespace sky::ios::test {

    void *GetWindowScene()
    {
        return (__bridge void *)hostScene;
    }

    bool WasConnectedBySceneDelegate()
    {
        return sceneConnected;
    }

    bool WasActivatedBySceneDelegate()
    {
        return sceneActivated;
    }

} // namespace sky::ios::test

@interface SkyIOSPlatformTestSceneDelegate : UIResponder <UIWindowSceneDelegate>
@end

@implementation SkyIOSPlatformTestSceneDelegate

- (void)scene:(UIScene *)scene willConnectToSession:(UISceneSession *)session options:(UISceneConnectionOptions *)options
{
    (void)session;
    (void)options;
    if (![scene isKindOfClass:[UIWindowScene class]]) {
        std::fputs("Test host requires a UIKit-created UIWindowScene\nSKY_IOS_TEST_EXIT=1\n", stderr);
        std::fflush(stderr);
        std::exit(1);
    }
    hostScene      = static_cast<UIWindowScene *>(scene);
    sceneConnected = true;
    // A minimal UIKit host window makes scene activation deterministic before
    // fixtures attach their separate engine-owned windows to this same scene.
    activationWindow                    = [[UIWindow alloc] initWithWindowScene:hostScene];
    activationWindow.rootViewController = [[UIViewController alloc] init];
    [activationWindow makeKeyAndVisible];
}

- (void)sceneDidBecomeActive:(UIScene *)scene
{
    if (scene != hostScene) {
        return;
    }
    sceneActivated = true;
    sky::ios::SetSceneActive((__bridge void *)scene, true);
    RunTestsAfterSceneActivation();
}

- (void)sceneWillResignActive:(UIScene *)scene
{
    sky::ios::SetSceneActive((__bridge void *)scene, false);
}

- (void)sceneWillEnterForeground:(UIScene *)scene
{
    sky::ios::SetSceneActive((__bridge void *)scene, false);
}

- (void)sceneDidEnterBackground:(UIScene *)scene
{
    sky::ios::SetSceneActive((__bridge void *)scene, false);
}

- (void)sceneDidDisconnect:(UIScene *)scene
{
    sky::ios::DisconnectScene((__bridge void *)scene);
}

@end

@interface SkyIOSPlatformTestDelegate : UIResponder <UIApplicationDelegate>
@end

@implementation SkyIOSPlatformTestDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    (void)application;
    (void)launchOptions;
    return YES;
}

- (UISceneConfiguration *)application:(UIApplication *)application
    configurationForConnectingSceneSession:(UISceneSession *)session
                                   options:(UISceneConnectionOptions *)options
{
    (void)application;
    (void)options;
    UISceneConfiguration *configuration = [[UISceneConfiguration alloc] initWithName:@"SkyEnginePlatformTests" sessionRole:session.role];
    configuration.sceneClass            = [UIWindowScene class];
    configuration.delegateClass         = [SkyIOSPlatformTestSceneDelegate class];
    return configuration;
}

@end

int main(int argc, char **argv)
{
    bool explicitFilter = std::getenv("GTEST_FILTER") != nullptr;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "--gtest_filter" || argument.starts_with("--gtest_filter=")) {
            explicitFilter = true;
        }
    }
    ::testing::InitGoogleTest(&argc, argv);
    if (!explicitFilter) {
        // Other Framework tests assume a host filesystem/process environment.
        ::testing::GTEST_FLAG(filter) = "IOSNativePlatformTest.*:IOSPlatformStateTest.*";
    }
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([SkyIOSPlatformTestDelegate class]));
    }
}

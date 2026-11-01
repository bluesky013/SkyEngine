//
// UIKit creates the test scene before any engine platform fixture runs.
//

#include "IOSPlatformTestHost.h"

#include <framework/platform/IOSSceneHost.h>
#include <framework/platform/PlatformBase.h>
#include <framework/window/NativeWindow.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>

#import <QuartzCore/CAMetalLayer.h>
#import <UIKit/UIKit.h>

@class SkyManualInputController;

namespace {

    UIWindowScene *hostScene        = nil;
    UIWindow      *activationWindow = nil;
    bool           sceneConnected   = false;
    bool           sceneActivated   = false;
    bool           testsScheduled   = false;
#if defined(SKY_IOS_TEST_MANUAL_INPUT) && SKY_IOS_TEST_MANUAL_INPUT
    bool manualInputMode = true;
#else
    bool manualInputMode = false;
#endif

    void StartManualInput(UIWindowScene *scene);
    void ShowManualInput(bool multiline, bool password);
    void StopManualInput();

    void RunTestsAfterSceneActivation()
    {
        if (manualInputMode && sceneConnected && sceneActivated) {
            UIWindowScene *scene = hostScene;
            [NSTimer scheduledTimerWithTimeInterval:0.0
                                            repeats:NO
                                              block:^(NSTimer *) {
                                                  if (scene == hostScene && sceneConnected && sceneActivated) {
                                                      StartManualInput(scene);
                                                  }
                                              }];
            return;
        }
        if (testsScheduled || !sceneConnected || !sceneActivated) {
            return;
        }
        testsScheduled = true;
        // Nested UI waits must still dispatch UIKit's main-queue transition completions.
        // Run from the native run loop instead of occupying the main dispatch queue.
        [NSTimer scheduledTimerWithTimeInterval:0.0
                                        repeats:NO
                                          block:^(NSTimer *) {
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
                                              // Let the device console drain completion markers before process exit.
                                              [NSTimer scheduledTimerWithTimeInterval:0.25
                                                                              repeats:NO
                                                                                block:^(NSTimer *) {
                                                                                    std::exit(result);
                                                                                }];
                                          }];
    }

} // namespace

@interface SkyManualInputController : UIViewController

@property (nonatomic, strong) UILabel *statusLabel;
@property (nonatomic, strong) UILabel *valueLabel;
@end

@implementation SkyManualInputController

- (void)viewDidLoad
{
    [super viewDidLoad];
    self.view.backgroundColor      = [UIColor systemBackgroundColor];
    UILabel *title                 = [[UILabel alloc] init];
    title.text                     = @"Sky Input Test";
    title.font                     = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle1];
    UILabel *instructions          = [[UILabel alloc] init];
    instructions.text              = @"Choose a mode. Try Chinese or emoji, then confirm, cancel, and reopen. Password content stays hidden.";
    instructions.font              = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    instructions.numberOfLines     = 0;
    self.statusLabel               = [[UILabel alloc] init];
    self.statusLabel.text          = @"Ready";
    self.statusLabel.numberOfLines = 0;
    self.statusLabel.font          = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    self.valueLabel                = [[UILabel alloc] init];
    self.valueLabel.text           = @"No committed text yet.";
    self.valueLabel.numberOfLines  = 0;
    self.valueLabel.font           = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    UIStackView *stack             = [[UIStackView alloc] init];
    stack.axis                     = UILayoutConstraintAxisVertical;
    stack.spacing                  = [UIFont preferredFontForTextStyle:UIFontTextStyleBody].lineHeight;
    [stack addArrangedSubview:title];
    [stack addArrangedSubview:instructions];
    NSArray<NSString *> *names   = @[ @"Single line", @"Multiline", @"Password" ];
    NSArray<NSString *> *actions = @[ @"openSingle:", @"openMultiline:", @"openPassword:" ];
    for (NSUInteger index = 0; index < names.count; ++index) {
        UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
        [button setTitle:names[index] forState:UIControlStateNormal];
        button.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
        [button addTarget:self action:NSSelectorFromString(actions[index]) forControlEvents:UIControlEventTouchUpInside];
        [stack addArrangedSubview:button];
    }
    [stack addArrangedSubview:self.statusLabel];
    [stack addArrangedSubview:self.valueLabel];
    stack.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:stack];
    UILayoutGuide *guide = self.view.layoutMarginsGuide;
    [NSLayoutConstraint activateConstraints:@[
        [stack.leadingAnchor constraintEqualToAnchor:guide.leadingAnchor], [stack.trailingAnchor constraintEqualToAnchor:guide.trailingAnchor],
        [stack.topAnchor constraintEqualToAnchor:guide.topAnchor], [stack.bottomAnchor constraintLessThanOrEqualToAnchor:guide.bottomAnchor]
    ]];
}

- (void)openSingle:(id)sender
{
    (void)sender;
    ShowManualInput(false, false);
}

- (void)openMultiline:(id)sender
{
    (void)sender;
    ShowManualInput(true, false);
}

- (void)openPassword:(id)sender
{
    (void)sender;
    ShowManualInput(false, true);
}

@end

namespace {

    class ManualInputFixture {
    public:
        ~ManualInputFixture()
        {
            closing = true;
            if (initialized) {
                auto *const platform = sky::Platform::Get();
                platform->StopFrameLoop();
                platform->SetFrameCallback({});
                platform->HideTextInputDialog();
                window.reset();
                platform->Shutdown();
            }
        }

        bool Attach(UIWindowScene *scene)
        {
            auto *const platform = sky::Platform::Get();
            if (!initialized) {
                if (!platform->Init({(__bridge void *)[UIApplication sharedApplication]})) {
                    return false;
                }
                initialized = true;
            }
            if (!sky::ios::ConnectScene((__bridge void *)scene)) {
                return false;
            }
            if (window == nullptr) {
                sky::NativeWindow::Descriptor descriptor;
                descriptor.titleName = "Sky Input Test";
                window.reset(sky::NativeWindow::Create(descriptor));
            }
            if (window == nullptr || window->GetNativeHandle() == nullptr) {
                return false;
            }
            CAMetalLayer *layer = (__bridge CAMetalLayer *)window->GetNativeHandle();
            if (![layer.delegate isKindOfClass:[UIView class]]) {
                return false;
            }
            UIView           *view   = static_cast<UIView *>(layer.delegate);
            UIViewController *parent = view.window.rootViewController;
            if (parent == nil) {
                return false;
            }
            if (controller == nil) {
                controller = [[SkyManualInputController alloc] init];
            }
            if (controller == nil) {
                return false;
            }
            if (controller.parentViewController != parent) {
                [controller willMoveToParentViewController:nil];
                [controller.view removeFromSuperview];
                [controller removeFromParentViewController];
                [parent addChildViewController:controller];
                controller.view.translatesAutoresizingMaskIntoConstraints = NO;
                [parent.view addSubview:controller.view];
                UILayoutGuide *guide = parent.view.safeAreaLayoutGuide;
                [NSLayoutConstraint activateConstraints:@[
                    [controller.view.leadingAnchor constraintEqualToAnchor:guide.leadingAnchor],
                    [controller.view.trailingAnchor constraintEqualToAnchor:guide.trailingAnchor],
                    [controller.view.topAnchor constraintEqualToAnchor:guide.topAnchor],
                    [controller.view.bottomAnchor constraintEqualToAnchor:guide.bottomAnchor]
                ]];
                [controller didMoveToParentViewController:parent];
            }
            platform->SetFrameCallback([] {
                bool exit = false;
                sky::Platform::Get()->PoolEvent(exit, sky::EventPollMode::NON_BLOCKING);
            });
            platform->StartFrameLoop();
            sky::ios::SetSceneActive((__bridge void *)scene, true);
            activationWindow.hidden = YES;
            [view.window makeKeyAndVisible];
            return true;
        }

        void Show(bool multiline, bool password)
        {
            if (closing || controller == nil) {
                return;
            }
            sky::NativeTextInputOptions options;
            options.title               = password ? "Password" : multiline ? "Multiline" : "Single line";
            options.placeholder         = "Try Chinese, emoji, and composed text";
            options.maxLength           = multiline ? 128 : 32;
            options.multiline           = multiline;
            options.password            = password;
            options.text                = password ? std::string{} : multiline ? multiText : singleText;
            controller.statusLabel.text = @"Opening input...";
            controller.valueLabel.text  = password ? @"Password content stays hidden." : @"Waiting for committed text.";
            const bool accepted =
                sky::Platform::Get()->ShowTextInputDialog(options, [this, multiline, password](const sky::NativeTextInputEvent &event) {
                    if (closing) {
                        return;
                    }
                    NSString *state = @"Unknown";
                    switch (event.type) {
                    case sky::NativeTextInputEventType::CHANGED: state = @"Changed"; break;
                    case sky::NativeTextInputEventType::CONFIRMED: state = @"Confirmed"; break;
                    case sky::NativeTextInputEventType::CANCELLED: state = @"Cancelled"; break;
                    case sky::NativeTextInputEventType::ERROR: state = @"Error"; break;
                    case sky::NativeTextInputEventType::END: controller.statusLabel.text = @"Invalid native input event."; return;
                    }
                    controller.statusLabel.text = [NSString stringWithFormat:@"%@: %@ (event %u, error %d)",
                                                                             password    ? @"Password"
                                                                             : multiline ? @"Multiline"
                                                                                         : @"Single line",
                                                                             state, ++events, event.nativeError];
                    if (password) {
                        controller.valueLabel.text = @"Password content stays hidden.";
                    } else {
                        (multiline ? multiText : singleText) = event.text;
                        controller.valueLabel.text =
                            [[NSString alloc] initWithBytes:event.text.data() length:event.text.size() encoding:NSUTF8StringEncoding] ?: @"";
                    }
                });
            if (!accepted) {
                controller.statusLabel.text = @"Input unavailable or busy. Close the current dialog and try again.";
            }
        }

    private:
        std::unique_ptr<sky::NativeWindow> window;
        SkyManualInputController          *controller = nil;
        std::string                        singleText;
        std::string                        multiText;
        uint32_t                           events      = 0;
        bool                               initialized = false;
        bool                               closing     = false;
    };

    // The process fixture outlives every native callback and scene reconnect.
    std::unique_ptr<ManualInputFixture> manualFixture;

    void StartManualInput(UIWindowScene *scene)
    {
        if (manualFixture == nullptr) {
            manualFixture = std::make_unique<ManualInputFixture>();
        }
        if (!manualFixture->Attach(scene)) {
            UILabel *error                           = [[UILabel alloc] initWithFrame:activationWindow.bounds];
            error.text                               = @"Unable to initialize Sky Input Test. Reopen the app to retry.";
            error.numberOfLines                      = 0;
            error.textAlignment                      = NSTextAlignmentCenter;
            activationWindow.rootViewController.view = error;
            activationWindow.hidden                  = NO;
        }
    }

    void ShowManualInput(bool multiline, bool password)
    {
        if (manualFixture != nullptr) {
            manualFixture->Show(multiline, password);
        }
    }

    void StopManualInput()
    {
        manualFixture.reset();
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
    if (manualInputMode) {
        sceneActivated = false;
    }
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
    if (manualInputMode && scene == hostScene) {
        sceneActivated = false;
    }
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
    // Automated fixtures inject disconnects while retaining the UIKit test scene.
    if (manualInputMode && scene == hostScene) {
        sceneConnected          = false;
        sceneActivated          = false;
        hostScene               = nil;
        activationWindow.hidden = YES;
        activationWindow        = nil;
    }
}

@end

@interface SkyIOSPlatformTestDelegate : UIResponder <UIApplicationDelegate>
@end

@implementation SkyIOSPlatformTestDelegate

- (void)applicationWillTerminate:(UIApplication *)application
{
    (void)application;
    StopManualInput();
}

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
        if (argument == "--sky-manual-input") {
            manualInputMode = true;
        }
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

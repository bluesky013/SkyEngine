//
// Run in a UIApplicationMain test host, on the UIKit main thread.
//

#include "../../../../framework/platform/ios/IOSPlatform.h"
#include "../../../../framework/platform/ios/IOSWindow.h"
#include "IOSPlatformTestHost.h"

#include <core/event/Event.h>
#include <framework/platform/IOSSceneHost.h>
#include <framework/window/NativeWindowManager.h>
#include <gtest/gtest.h>

#import <QuartzCore/CAMetalLayer.h>
#import <UIKit/UIKit.h>

namespace {

    class IOSNativePlatformTest : public ::testing::Test {
    protected:
        void SetUp() override
        {
            ASSERT_TRUE([NSThread isMainThread]);
            ASSERT_TRUE(sky::Platform::Get()->Init({(__bridge void *)[UIApplication sharedApplication]}));
            platform = static_cast<sky::IOSPlatform *>(sky::Platform::Get()->GetImpl());
            scene    = (__bridge UIWindowScene *)sky::ios::test::GetWindowScene();
            ASSERT_NE(scene, nil);
            ASSERT_TRUE(sky::ios::test::WasConnectedBySceneDelegate());
            ASSERT_TRUE([[UIApplication sharedApplication].connectedScenes containsObject:scene]);
            ASSERT_TRUE(sky::ios::ConnectScene((__bridge void *)scene));
            window = std::make_unique<sky::IOSWindow>();
            ASSERT_TRUE(window->Init({}));
            sky::ios::SetSceneActive((__bridge void *)scene, true);
        }

        void TearDown() override
        {
            if (platform != nullptr) {
                platform->StopFrameLoop();
                platform->SetFrameCallback({});
            }
            window.reset();
            sky::Platform::Get()->Shutdown();
        }

        UIWindowScene                  *scene    = nil;
        sky::IOSPlatform               *platform = nullptr;
        std::unique_ptr<sky::IOSWindow> window;
    };

    class KeyboardCapture : public sky::IKeyboardEvent {
    public:
        KeyboardCapture()
        {
            sky::Event<sky::IKeyboardEvent>::Connect(this);
        }
        ~KeyboardCapture()
        {
            sky::Event<sky::IKeyboardEvent>::DisConnect(this);
        }
        void OnTextInput(sky::WindowID id, const char *value) override
        {
            winId = id;
            text += value;
        }
        void OnKeyDown(const sky::KeyboardEvent &event) override
        {
            if (event.scanCode == sky::ScanCode::KEY_BACKSPACE) {
                ++backspaceDown;
            }
        }
        void OnKeyUp(const sky::KeyboardEvent &event) override
        {
            if (event.scanCode == sky::ScanCode::KEY_BACKSPACE) {
                ++backspaceUp;
            }
        }
        sky::WindowID winId = sky::INVALID_WIN_ID;
        std::string   text;
        uint32_t      backspaceDown = 0;
        uint32_t      backspaceUp   = 0;
    };

    class WindowCapture : public sky::IWindowEvent {
    public:
        explicit WindowCapture(const sky::NativeWindow *window)
        {
            sky::Event<sky::IWindowEvent>::Connect(window, this);
        }
        ~WindowCapture() override
        {
            sky::Event<sky::IWindowEvent>::DisConnect(this);
        }
        void OnWindowResize(const sky::WindowResizeEvent &event) override
        {
            resizedId = event.winID;
            ++resizeCount;
        }
        void OnWindowClose(const sky::NativeWindow *window) override
        {
            closedId = window->GetWinId();
            ++closeCount;
        }
        void OnSurfaceChanged(const sky::NativeWindow *window) override
        {
            surfaceGone = window->GetNativeHandle() == nullptr;
            ++surfaceChanges;
        }
        sky::WindowID resizedId      = sky::INVALID_WIN_ID;
        sky::WindowID closedId       = sky::INVALID_WIN_ID;
        uint32_t      closeCount     = 0;
        uint32_t      resizeCount    = 0;
        bool          surfaceGone    = false;
        uint32_t      surfaceChanges = 0;
    };

    class DeleteWindowCapture : public sky::IWindowEvent {
    public:
        DeleteWindowCapture(std::unique_ptr<sky::IOSWindow> &window, bool onFocus) : owner(window), deleteOnFocus(onFocus)
        {
            sky::Event<sky::IWindowEvent>::Connect(window.get(), this);
        }
        ~DeleteWindowCapture() override
        {
            sky::Event<sky::IWindowEvent>::DisConnect(this);
        }
        void OnWindowClose(const sky::NativeWindow *) override
        {
            if (!deleteOnFocus) {
                owner.reset();
            }
        }
        void OnFocusChanged(bool focused) override
        {
            if (deleteOnFocus && !focused) {
                owner.reset();
            }
        }

    private:
        std::unique_ptr<sky::IOSWindow> &owner;
        bool                             deleteOnFocus;
    };

} // namespace

TEST_F(IOSNativePlatformTest, RealSceneDelegateConnectionMatchesSingleSceneManifest)
{
    ASSERT_TRUE(sky::ios::test::WasConnectedBySceneDelegate());
    ASSERT_TRUE(sky::ios::test::WasActivatedBySceneDelegate());
    EXPECT_TRUE([scene isKindOfClass:[UIWindowScene class]]);
    EXPECT_EQ(scene.activationState, UISceneActivationStateForegroundActive);
    EXPECT_TRUE([[UIApplication sharedApplication].connectedScenes containsObject:scene]);
    EXPECT_STREQ(NSStringFromClass([scene.delegate class]).UTF8String, "SkyIOSPlatformTestSceneDelegate");
    EXPECT_FALSE([[UIApplication sharedApplication].delegate respondsToSelector:@selector(window)]);
    NSDictionary *manifest = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"UIApplicationSceneManifest"];
    ASSERT_NE(manifest, nil);
    EXPECT_FALSE([manifest[@"UIApplicationSupportsMultipleScenes"] boolValue]);
    NSArray *configurations = manifest[@"UISceneConfigurations"][@"UIWindowSceneSessionRoleApplication"];
    ASSERT_EQ(configurations.count, 1U);
    EXPECT_STREQ([configurations.firstObject[@"UISceneDelegateClassName"] UTF8String], "SkyIOSPlatformTestSceneDelegate");
    EXPECT_STREQ([configurations.firstObject[@"UISceneClassName"] UTF8String], "UIWindowScene");
    EXPECT_EQ(window->GetWindowScene(), (__bridge void *)scene);
    CAMetalLayer *layer = (__bridge CAMetalLayer *)window->GetNativeHandle();
    UIView       *view  = static_cast<UIView *>(layer.delegate);
    ASSERT_NE(view.window, nil);
    EXPECT_EQ(view.window.windowScene, scene);
    EXPECT_FLOAT_EQ(window->GetDpiScale(), static_cast<float>(scene.screen.nativeScale));
    EXPECT_EQ(window->GetWidth(), sky::ios::ToFramebufferExtent(scene.coordinateSpace.bounds.size.width, window->GetDpiScale()));
    EXPECT_EQ(window->GetHeight(), sky::ios::ToFramebufferExtent(scene.coordinateSpace.bounds.size.height, window->GetDpiScale()));
}

TEST_F(IOSNativePlatformTest, ProcessInitCannotCreateALegacyWindowWithoutSceneConnection)
{
    window.reset();
    sky::Platform::Get()->Shutdown();
    ASSERT_TRUE(sky::Platform::Get()->Init({(__bridge void *)[UIApplication sharedApplication]}));
    platform     = static_cast<sky::IOSPlatform *>(sky::Platform::Get()->GetImpl());
    auto unbound = std::make_unique<sky::IOSWindow>();
    EXPECT_FALSE(unbound->Init({}));
    EXPECT_EQ(unbound->GetNativeHandle(), nullptr);
    EXPECT_EQ(platform->GetMainWinHandle(), nullptr);
    EXPECT_FALSE(platform->IsFrameActive());
}

TEST_F(IOSNativePlatformTest, SceneDelegateLifecycleGatesFramesAndEpochWithoutGlobalNotificationFallback)
{
    uint32_t frames = 0;
    platform->SetFrameCallback([&frames]() { ++frames; });
    platform->StartFrameLoop();
    platform->OnDisplayTick();
    EXPECT_EQ(frames, 1U);
    const uint64_t activeEpoch = platform->GetFrameEpoch();
    [scene.delegate sceneWillResignActive:scene];
    platform->OnDisplayTick();
    EXPECT_FALSE(platform->IsFrameActive());
    EXPECT_EQ(frames, 1U);
    EXPECT_GT(platform->GetFrameEpoch(), activeEpoch);
    const uint64_t inactiveEpoch = platform->GetFrameEpoch();
    [[NSNotificationCenter defaultCenter] postNotificationName:UIApplicationDidBecomeActiveNotification object:[UIApplication sharedApplication]];
    [[NSNotificationCenter defaultCenter] postNotificationName:UIApplicationWillEnterForegroundNotification object:[UIApplication sharedApplication]];
    EXPECT_FALSE(platform->IsFrameActive());
    EXPECT_EQ(platform->GetFrameEpoch(), inactiveEpoch);
    [scene.delegate sceneDidEnterBackground:scene];
    [scene.delegate sceneWillEnterForeground:scene];
    platform->OnDisplayTick();
    EXPECT_EQ(frames, 1U);
    [scene.delegate sceneDidBecomeActive:scene];
    platform->OnDisplayTick();
    EXPECT_TRUE(platform->IsFrameActive());
    EXPECT_EQ(frames, 2U);
    EXPECT_GT(platform->GetFrameEpoch(), inactiveEpoch);
    const uint64_t resumedEpoch = platform->GetFrameEpoch();
    [[NSNotificationCenter defaultCenter] postNotificationName:UIApplicationWillResignActiveNotification object:[UIApplication sharedApplication]];
    [[NSNotificationCenter defaultCenter] postNotificationName:UIApplicationDidEnterBackgroundNotification object:[UIApplication sharedApplication]];
    EXPECT_TRUE(platform->IsFrameActive());
    EXPECT_EQ(platform->GetFrameEpoch(), resumedEpoch);
}

TEST_F(IOSNativePlatformTest, ForeignSceneCallbacksCannotChangeTheConnectedWindow)
{
    NSObject      *foreign  = [[NSObject alloc] init];
    void          *identity = (__bridge void *)foreign;
    void          *handle   = window->GetNativeHandle();
    const uint64_t epoch    = platform->GetFrameEpoch();
    EXPECT_FALSE(sky::ios::ConnectScene(nullptr));
    EXPECT_FALSE(sky::ios::ConnectScene(identity));
    sky::ios::SetSceneActive(identity, false);
    sky::ios::DisconnectScene(identity);
    EXPECT_TRUE(platform->IsFrameActive());
    EXPECT_EQ(platform->GetFrameEpoch(), epoch);
    EXPECT_EQ(window->GetNativeHandle(), handle);
    EXPECT_EQ(window->GetWindowScene(), (__bridge void *)scene);
}

TEST_F(IOSNativePlatformTest, DisconnectAndReconnectPreserveWindowIdentityAndWaitForSceneActivation)
{
    WindowCapture       capture(window.get());
    const sky::WindowID id     = window->GetWinId();
    uint32_t            frames = 0;
    platform->SetFrameCallback([&frames]() { ++frames; });
    platform->StartFrameLoop();
    platform->OnDisplayTick();
    ASSERT_EQ(frames, 1U);
    const uint64_t epoch = platform->GetFrameEpoch();
    [scene.delegate sceneDidDisconnect:scene];
    EXPECT_EQ(window->GetNativeHandle(), nullptr);
    EXPECT_EQ(window->GetWindowScene(), nullptr);
    EXPECT_EQ(platform->GetMainWinHandle(), nullptr);
    EXPECT_FALSE(platform->IsFrameActive());
    EXPECT_GT(platform->GetFrameEpoch(), epoch);
    EXPECT_EQ(window->GetWinId(), id);
    EXPECT_EQ(sky::NativeWindowManager::Get()->GetWindowByID(id), window.get());
    EXPECT_TRUE(capture.surfaceGone);
    EXPECT_EQ(capture.closeCount, 0U);
    platform->OnDisplayTick();
    EXPECT_EQ(frames, 1U);
    ASSERT_TRUE(sky::ios::ConnectScene((__bridge void *)scene));
    EXPECT_NE(window->GetNativeHandle(), nullptr);
    EXPECT_EQ(window->GetWindowScene(), (__bridge void *)scene);
    EXPECT_EQ(window->GetWinId(), id);
    EXPECT_FALSE(platform->IsFrameActive());
    platform->OnDisplayTick();
    EXPECT_EQ(frames, 1U);
    EXPECT_FALSE(capture.surfaceGone);
    EXPECT_GE(capture.surfaceChanges, 2U);
    [scene.delegate sceneDidBecomeActive:scene];
    platform->OnDisplayTick();
    EXPECT_EQ(frames, 2U);
}

TEST_F(IOSNativePlatformTest, ExplicitStopIsNotRevivedBySceneReconnectOrActivation)
{
    uint32_t frames = 0;
    platform->SetFrameCallback([&frames]() { ++frames; });
    platform->StartFrameLoop();
    platform->OnDisplayTick();
    ASSERT_EQ(frames, 1U);
    platform->StopFrameLoop();
    sky::ios::DisconnectScene((__bridge void *)scene);
    ASSERT_TRUE(sky::ios::ConnectScene((__bridge void *)scene));
    [scene.delegate sceneDidBecomeActive:scene];
    platform->OnDisplayTick();
    EXPECT_EQ(frames, 1U);
    platform->StartFrameLoop();
    platform->OnDisplayTick();
    EXPECT_EQ(frames, 2U);
}

TEST_F(IOSNativePlatformTest, DetachedViewCannotPublishLateInputToTheReconnectedWindow)
{
    KeyboardCapture keyboard;
    CAMetalLayer   *layer       = (__bridge CAMetalLayer *)window->GetNativeHandle();
    UIView         *oldView     = static_cast<UIView *>(layer.delegate);
    UITextView     *oldTextView = static_cast<UITextView *>(oldView.subviews.firstObject);
    ASSERT_NE(oldTextView, nil);
    const sky::WindowID id = window->GetWinId();
    sky::ios::DisconnectScene((__bridge void *)scene);
    ASSERT_TRUE(sky::ios::ConnectScene((__bridge void *)scene));
    sky::ios::SetSceneActive((__bridge void *)scene, true);
    WindowCapture  windowEvents(window.get());
    const uint32_t liveWidth  = window->GetWidth();
    const uint32_t liveHeight = window->GetHeight();
    const void    *liveHandle = window->GetNativeHandle();
    // A retained old view must not resize the new scene's engine surface.
    // A distinct extent makes stale-owner callbacks observable.
    oldView.frame = CGRectMake(0, 0, 17, 29);
    [oldView layoutSubviews];
    EXPECT_EQ(windowEvents.resizeCount, 0U);
    EXPECT_EQ(window->GetWidth(), liveWidth);
    EXPECT_EQ(window->GetHeight(), liveHeight);
    EXPECT_EQ(window->GetNativeHandle(), liveHandle);
    EXPECT_EQ(oldTextView.delegate, nil);
    [oldTextView insertText:@"ignored"];
    [oldTextView deleteBackward];
    EXPECT_TRUE(keyboard.text.empty());
    EXPECT_EQ(keyboard.backspaceDown, 0U);
    EXPECT_EQ(keyboard.backspaceUp, 0U);
    EXPECT_EQ(window->GetWinId(), id);
    EXPECT_EQ(window->GetWindowScene(), (__bridge void *)scene);
}

TEST_F(IOSNativePlatformTest, SurfaceIdentityFramebufferExtentAndCloseAreConsistent)
{
    WindowCapture capture(window.get());
    EXPECT_TRUE(window->IsMainWindow());
    EXPECT_NE(window->GetWinId(), sky::INVALID_WIN_ID);
    EXPECT_EQ(sky::NativeWindowManager::Get()->GetMainWindow(), window.get());
    CAMetalLayer *layer = (__bridge CAMetalLayer *)window->GetNativeHandle();
    UIView       *view  = static_cast<UIView *>(layer.delegate);
    view.frame          = CGRectMake(0, 0, 200, 100);
    [view layoutSubviews];
    EXPECT_EQ(window->GetWidth(), static_cast<uint32_t>(layer.drawableSize.width));
    EXPECT_EQ(window->GetHeight(), static_cast<uint32_t>(layer.drawableSize.height));
    EXPECT_EQ(window->GetWidth(), sky::ios::ToFramebufferExtent(200, window->GetDpiScale()));
    EXPECT_EQ(capture.resizedId, window->GetWinId());
    window->Close();
    window->Close();
    EXPECT_EQ(window->GetNativeHandle(), nullptr);
    EXPECT_EQ(platform->GetMainWinHandle(), nullptr);
    EXPECT_FALSE(platform->IsFrameActive());
    EXPECT_EQ(sky::NativeWindowManager::Get()->GetMainWindow(), nullptr);
    EXPECT_EQ(capture.closeCount, 1U);
    EXPECT_EQ(capture.closedId, window->GetWinId());
    EXPECT_TRUE(capture.surfaceGone);
}

TEST_F(IOSNativePlatformTest, UTF8CommitAndBackspaceUseNativeTextInput)
{
    KeyboardCapture capture;
    platform->SetTextInputRect(30, 60, 3, 9);
    CAMetalLayer *layer    = (__bridge CAMetalLayer *)window->GetNativeHandle();
    UIView       *view     = static_cast<UIView *>(layer.delegate);
    UITextView   *textView = static_cast<UITextView *>(view.subviews.firstObject);
    ASSERT_NE(textView, nil);
    EXPECT_DOUBLE_EQ(textView.frame.origin.x, 30.0 / window->GetDpiScale());
    EXPECT_DOUBLE_EQ(textView.frame.origin.y, 60.0 / window->GetDpiScale());
    textView.text = @"\u4f60\u597d\U0001f642";
    [textView.delegate textViewDidChange:textView];
    EXPECT_EQ(capture.text, "\xe4\xbd\xa0\xe5\xa5\xbd\xf0\x9f\x99\x82");
    EXPECT_EQ(capture.winId, window->GetWinId());
    EXPECT_EQ(textView.text.length, 0U);
    [textView deleteBackward];
    EXPECT_EQ(capture.backspaceDown, 1U);
    EXPECT_EQ(capture.backspaceUp, 1U);
}

TEST_F(IOSNativePlatformTest, NativeClipboardPathsAndUnsupportedProcessContract)
{
    platform->SetClipBoardText("\xe4\xbd\xa0\xe5\xa5\xbd");
    char *text = platform->GetClipBoardText();
    ASSERT_NE(text, nullptr);
    EXPECT_STREQ(text, "\xe4\xbd\xa0\xe5\xa5\xbd");
    platform->FreeClipBoardText(text);
    EXPECT_FALSE(platform->GetInternalPath().empty());
    EXPECT_FALSE(platform->GetBundlePath().empty());
    EXPECT_FALSE(platform->GetUserConfigPath().empty());
    EXPECT_NE(platform->GetBundleFileSystem(), nullptr);
    std::string output;
    EXPECT_FALSE(platform->RunCmd("ignored", output));
    EXPECT_TRUE(output.empty());
}

TEST_F(IOSNativePlatformTest, MarkedTextStaysNativeUntilCommittedAndStopDismissesKeyboard)
{
    KeyboardCapture capture;
    platform->StartTextInput();
    CAMetalLayer *layer    = (__bridge CAMetalLayer *)window->GetNativeHandle();
    UIView       *view     = static_cast<UIView *>(layer.delegate);
    UITextView   *textView = static_cast<UITextView *>(view.subviews.firstObject);
    ASSERT_NE(textView, nil);
    ASSERT_TRUE(textView.isFirstResponder);
    [textView setMarkedText:@"ni" selectedRange:NSMakeRange(2, 0)];
    ASSERT_NE(textView.markedTextRange, nil);
    [textView.delegate textViewDidChange:textView];
    EXPECT_TRUE(capture.text.empty());
    [textView unmarkText];
    [textView.delegate textViewDidChange:textView];
    EXPECT_EQ(capture.text, "ni");
    [textView setMarkedText:@"unfinished" selectedRange:NSMakeRange(10, 0)];
    platform->StopTextInput();
    EXPECT_FALSE(textView.isFirstResponder);
    EXPECT_EQ(textView.markedTextRange, nil);
    EXPECT_EQ(textView.text.length, 0U);
    EXPECT_EQ(capture.text, "ni");
}

TEST_F(IOSNativePlatformTest, CloseAndFocusListenersMayDeleteWindowWithoutLiveNativeCallbacks)
{
    KeyboardCapture     keyboard;
    CAMetalLayer       *layer        = (__bridge CAMetalLayer *)window->GetNativeHandle();
    UIView             *retainedView = static_cast<UIView *>(layer.delegate);
    const sky::WindowID id           = window->GetWinId();
    {
        DeleteWindowCapture capture(window, false);
        window->Close();
        EXPECT_EQ(window, nullptr);
    }
    EXPECT_EQ(platform->GetMainWinHandle(), nullptr);
    EXPECT_EQ(sky::NativeWindowManager::Get()->GetWindowByID(id), nullptr);
    [retainedView layoutSubviews];
    UITextView *textView = static_cast<UITextView *>(retainedView.subviews.firstObject);
    EXPECT_EQ(textView.delegate, nil);
    [textView insertText:@"ignored"];
    EXPECT_TRUE(keyboard.text.empty());

    window = std::make_unique<sky::IOSWindow>();
    ASSERT_TRUE(window->Init({}));
    sky::ios::SetSceneActive((__bridge void *)scene, true);
    const sky::WindowID secondId = window->GetWinId();
    DeleteWindowCapture capture(window, true);
    window->SetFocused(false);
    EXPECT_EQ(window, nullptr);
    EXPECT_EQ(platform->GetMainWinHandle(), nullptr);
    EXPECT_EQ(sky::NativeWindowManager::Get()->GetWindowByID(secondId), nullptr);
}

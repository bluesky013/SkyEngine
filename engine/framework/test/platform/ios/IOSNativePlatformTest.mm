//
// Run in a UIApplicationMain test host, on the UIKit main thread.
//

#include "IOSPlatform.h"
#include "IOSPlatformTestHost.h"
#include "IOSWindow.h"

#include <core/event/Event.h>
#include <framework/platform/IOSSceneHost.h>
#include <framework/platform/NativeTextInput.h>
#include <framework/window/NativeWindowManager.h>
#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#import <QuartzCore/CAMetalLayer.h>
#import <QuartzCore/CATransaction.h>
#import <UIKit/UIKit.h>

namespace {

    UIView *FindDialogSubview(UIView *view, Class type)
    {
        if ([view isKindOfClass:type]) {
            return view;
        }
        for (UIView *child in view.subviews) {
            if (UIView *found = FindDialogSubview(child, type)) {
                return found;
            }
        }
        return nil;
    }

    UITextField *DialogField(UINavigationController *navigation)
    {
        return static_cast<UITextField *>(FindDialogSubview(navigation.topViewController.view, [UITextField class]));
    }

    UITextView *DialogEditor(UINavigationController *navigation)
    {
        return static_cast<UITextView *>(FindDialogSubview(navigation.topViewController.view, [UITextView class]));
    }

    bool InvokeDialogAction(UIBarButtonItem *item)
    {
        return item != nil && item.action != nullptr &&
               [[UIApplication sharedApplication] sendAction:item.action to:item.target from:item forEvent:nil];
    }

    class IOSNativePlatformTest : public ::testing::Test {
    protected:
        void SetUp() override
        {
            ASSERT_TRUE([NSThread isMainThread]);
            // Nested UI waits run before UIApplication's outer animation transaction commits.
            // Suppress timing only; presented-window and first-responder checks remain mandatory.
            animationsWereEnabled = [UIView areAnimationsEnabled];
            controlsAnimations    = true;
            [UIView setAnimationsEnabled:NO];
            callbackThread = std::this_thread::get_id();
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
                platform->SetSceneActive((__bridge void *)scene, false);
                platform->HideTextInputDialog();
                platform->StopFrameLoop();
                platform->SetFrameCallback({});
            }
            window.reset();
            sky::Platform::Get()->Shutdown();
            [CATransaction flush];
            if (controlsAnimations) {
                [UIView setAnimationsEnabled:animationsWereEnabled];
            }
        }

        bool PumpUntil(const std::function<bool()> &condition)
        {
            NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:5.0];
            while (!condition()) {
                if ([deadline timeIntervalSinceNow] <= 0.0) {
                    return false;
                }
                [CATransaction flush];
                [[NSRunLoop mainRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
                bool exit = false;
                platform->PollEvent(exit, sky::EventPollMode::NON_BLOCKING);
            }
            return true;
        }

        UIViewController *RootController() const
        {
            if (window == nullptr || window->GetNativeHandle() == nullptr) {
                return nil;
            }
            CAMetalLayer *layer = (__bridge CAMetalLayer *)window->GetNativeHandle();
            UIView       *view  = static_cast<UIView *>(layer.delegate);
            return view.window.rootViewController;
        }

        UINavigationController *WaitForDialog(UIViewController *presenter)
        {
            const bool ready = PumpUntil([presenter] {
                UIViewController *presented = presenter.presentedViewController;
                if (![presented isKindOfClass:[UINavigationController class]] || presented.isBeingPresented) {
                    return false;
                }
                UINavigationController *navigation = static_cast<UINavigationController *>(presented);
                UIResponder            *input      = DialogField(navigation);
                if (input == nil) {
                    input = DialogEditor(navigation);
                }
                return navigation.presentingViewController != nil && input.isFirstResponder;
            });
            if (!ready) {
                UIViewController       *presented    = presenter.presentedViewController;
                const bool              isNavigation = [presented isKindOfClass:[UINavigationController class]];
                UINavigationController *navigation   = isNavigation ? static_cast<UINavigationController *>(presented) : nil;
                UITextField            *field        = DialogField(navigation);
                UITextView             *editor       = DialogEditor(navigation);
                ADD_FAILURE() << "Native dialog did not become the first responder; terminal events=" << DialogTerminalCount()
                              << ", native error=" << (dialogEvents.empty() ? 0 : dialogEvents.back().nativeError) << ", navigation=" << isNavigation
                              << ", presenting=" << (presented.presentingViewController != nil) << ", transition=" << presented.isBeingPresented
                              << ", view window=" << (presented.view.window != nil) << ", field=" << (field != nil)
                              << ", field responder=" << field.isFirstResponder << ", editor=" << (editor != nil)
                              << ", editor responder=" << editor.isFirstResponder;
                return nil;
            }
            return static_cast<UINavigationController *>(presenter.presentedViewController);
        }

        sky::NativeTextInputCallback RecordDialog()
        {
            return [this](const sky::NativeTextInputEvent &event) { RecordDialogEvent(event); };
        }

        void RecordDialogEvent(const sky::NativeTextInputEvent &event)
        {
            callbacksOnOwner = callbacksOnOwner && [NSThread isMainThread] && callbackThread == std::this_thread::get_id();
            dialogEvents.push_back(event);
        }

        UINavigationController *OpenDialog(const sky::NativeTextInputOptions &options, sky::NativeTextInputCallback callback = {})
        {
            UIViewController *presenter = RootController();
            if (presenter == nil || !PumpUntil([presenter] { return presenter.view.window != nil && presenter.presentedViewController == nil; })) {
                ADD_FAILURE() << "Native dialog presenter is unavailable";
                return nil;
            }
            if (!callback) {
                callback = RecordDialog();
            }
            if (!platform->ShowTextInputDialog(options, std::move(callback))) {
                ADD_FAILURE() << "Native dialog request was rejected";
                return nil;
            }
            return WaitForDialog(presenter);
        }

        size_t DialogEventCount(sky::NativeTextInputEventType type) const
        {
            size_t count = 0;
            for (const auto &event : dialogEvents) {
                if (event.type == type) {
                    ++count;
                }
            }
            return count;
        }

        size_t DialogTerminalCount() const
        {
            return dialogEvents.size() - DialogEventCount(sky::NativeTextInputEventType::CHANGED);
        }

        UIWindowScene                  *scene    = nil;
        sky::IOSPlatform               *platform = nullptr;
        std::unique_ptr<sky::IOSWindow> window;
        // Callback storage survives fatal assertions until teardown has collected the terminal event.
        std::vector<sky::NativeTextInputEvent> dialogEvents;
        std::thread::id                        callbackThread;
        bool                                   callbacksOnOwner      = true;
        bool                                   reopenAttempted       = false;
        bool                                   reopenAccepted        = false;
        bool                                   nativeConfirmAccepted = false;
        BOOL                                   animationsWereEnabled = YES;
        bool                                   controlsAnimations    = false;
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

TEST_F(IOSNativePlatformTest, DialogSingleLineCountsUnicodeScalarsAndConfirmsOnceOnOwnerThread)
{
    sky::NativeTextInputOptions options;
    options.title                      = "Unicode input";
    options.placeholder                = "Value";
    options.maxLength                  = 3;
    UIViewController       *presenter  = RootController();
    UINavigationController *navigation = OpenDialog(options);
    ASSERT_NE(navigation, nil);
    UITextField *field = DialogField(navigation);
    ASSERT_NE(field, nil);
    EXPECT_EQ(DialogEditor(navigation), nil);
    EXPECT_FALSE(field.secureTextEntry);
    EXPECT_FALSE(platform->ShowTextInputDialog(options, RecordDialog()));
    NSString *accepted = @"\U0001F642\u4E2Da";
    ASSERT_TRUE([field.delegate textField:field shouldChangeCharactersInRange:NSMakeRange(0, 0) replacementString:accepted]);
    field.text = accepted;
    [field sendActionsForControlEvents:UIControlEventEditingChanged];
    [field sendActionsForControlEvents:UIControlEventEditingChanged];
    ASSERT_EQ(DialogEventCount(sky::NativeTextInputEventType::CHANGED), 1U);
    EXPECT_EQ(dialogEvents.front().text, "\xF0\x9F\x99\x82\xE4\xB8\xAD"
                                         "a");
    EXPECT_FALSE([field.delegate textField:field shouldChangeCharactersInRange:NSMakeRange(field.text.length, 0) replacementString:@"x"]);
    field.text = [accepted stringByAppendingString:@"x"];
    [field sendActionsForControlEvents:UIControlEventEditingChanged];
    EXPECT_TRUE([field.text isEqualToString:accepted]);
    EXPECT_EQ(DialogEventCount(sky::NativeTextInputEventType::CHANGED), 1U);
    ASSERT_TRUE(InvokeDialogAction(navigation.topViewController.navigationItem.rightBarButtonItem));
    EXPECT_TRUE(InvokeDialogAction(navigation.topViewController.navigationItem.rightBarButtonItem));
    ASSERT_TRUE(PumpUntil([this, presenter] { return DialogTerminalCount() == 1 && presenter.presentedViewController == nil; }));
    EXPECT_EQ(dialogEvents.back().type, sky::NativeTextInputEventType::CONFIRMED);
    EXPECT_EQ(dialogEvents.back().text, "\xF0\x9F\x99\x82\xE4\xB8\xAD"
                                        "a");
    EXPECT_EQ(dialogEvents.back().nativeError, 0);
    EXPECT_TRUE(callbacksOnOwner);
}

TEST_F(IOSNativePlatformTest, DialogMultilineUsesNativeEditorAndCancelsWithLatestSnapshot)
{
    sky::NativeTextInputOptions options;
    options.multiline                  = true;
    options.placeholder                = "Notes";
    UIViewController       *presenter  = RootController();
    UINavigationController *navigation = OpenDialog(options);
    ASSERT_NE(navigation, nil);
    UITextView *editor = DialogEditor(navigation);
    ASSERT_NE(editor, nil);
    EXPECT_EQ(DialogField(navigation), nil);
    editor.text = @"line one\n\u4E8C";
    [editor.delegate textViewDidChange:editor];
    [editor.delegate textViewDidChange:editor];
    ASSERT_EQ(DialogEventCount(sky::NativeTextInputEventType::CHANGED), 1U);
    ASSERT_TRUE(InvokeDialogAction(navigation.topViewController.navigationItem.leftBarButtonItem));
    EXPECT_TRUE(InvokeDialogAction(navigation.topViewController.navigationItem.leftBarButtonItem));
    ASSERT_TRUE(PumpUntil([this, presenter] { return DialogTerminalCount() == 1 && presenter.presentedViewController == nil; }));
    EXPECT_EQ(dialogEvents.back().type, sky::NativeTextInputEventType::CANCELLED);
    EXPECT_EQ(dialogEvents.back().text, "line one\n\xE4\xBA\x8C");
    EXPECT_EQ(dialogEvents.back().nativeError, 0);
    EXPECT_TRUE(callbacksOnOwner);
}

TEST_F(IOSNativePlatformTest, DialogPasswordUsesSecureEntryWithoutAutocorrection)
{
    sky::NativeTextInputOptions options;
    options.password                   = true;
    options.text                       = "fixture";
    UIViewController       *presenter  = RootController();
    UINavigationController *navigation = OpenDialog(options);
    ASSERT_NE(navigation, nil);
    UITextField *field = DialogField(navigation);
    ASSERT_NE(field, nil);
    EXPECT_TRUE(field.secureTextEntry);
    EXPECT_EQ(field.autocorrectionType, UITextAutocorrectionTypeNo);
    EXPECT_EQ(field.autocapitalizationType, UITextAutocapitalizationTypeNone);
    EXPECT_TRUE([field.textContentType isEqualToString:UITextContentTypePassword]);
    field.text = @"updated fixture";
    [field sendActionsForControlEvents:UIControlEventEditingChanged];
    ASSERT_TRUE(InvokeDialogAction(navigation.topViewController.navigationItem.leftBarButtonItem));
    ASSERT_TRUE(PumpUntil([this, presenter] { return DialogTerminalCount() == 1 && presenter.presentedViewController == nil; }));
    EXPECT_EQ(dialogEvents.back().type, sky::NativeTextInputEventType::CANCELLED);
    EXPECT_EQ(dialogEvents.back().text, "updated fixture");
    EXPECT_TRUE(callbacksOnOwner);
}

TEST_F(IOSNativePlatformTest, DialogMarkedTextIsExcludedUntilNativeCompositionCommits)
{
    sky::NativeTextInputOptions options;
    options.text                       = "A";
    options.maxLength                  = 3;
    UIViewController       *presenter  = RootController();
    UINavigationController *navigation = OpenDialog(options);
    ASSERT_NE(navigation, nil);
    UITextField *field = DialogField(navigation);
    ASSERT_NE(field, nil);
    field.selectedTextRange = [field textRangeFromPosition:field.endOfDocument toPosition:field.endOfDocument];
    [field setMarkedText:@"\u4E2D\u6587\u4EBA" selectedRange:NSMakeRange(3, 0)];
    ASSERT_NE(field.markedTextRange, nil);
    [field sendActionsForControlEvents:UIControlEventEditingChanged];
    EXPECT_EQ(DialogEventCount(sky::NativeTextInputEventType::CHANGED), 0U);
    EXPECT_EQ(DialogTerminalCount(), 0U);
    EXPECT_TRUE([field.delegate textField:field shouldChangeCharactersInRange:NSMakeRange(field.text.length, 0) replacementString:@"x"]);
    [field unmarkText];
    [field sendActionsForControlEvents:UIControlEventEditingChanged];
    EXPECT_TRUE([field.text isEqualToString:@"A"]);
    EXPECT_EQ(DialogEventCount(sky::NativeTextInputEventType::CHANGED), 0U);
    field.text = @"A\U0001F642";
    [field sendActionsForControlEvents:UIControlEventEditingChanged];
    ASSERT_EQ(DialogEventCount(sky::NativeTextInputEventType::CHANGED), 1U);
    ASSERT_TRUE(InvokeDialogAction(navigation.topViewController.navigationItem.rightBarButtonItem));
    ASSERT_TRUE(PumpUntil([this, presenter] { return DialogTerminalCount() == 1 && presenter.presentedViewController == nil; }));
    EXPECT_EQ(dialogEvents.back().type, sky::NativeTextInputEventType::CONFIRMED);
    EXPECT_EQ(dialogEvents.back().text, "A\xF0\x9F\x99\x82");
}

TEST_F(IOSNativePlatformTest, DialogTerminalCanReopenWhileRetiredControllerCallbacksRemainInert)
{
    sky::NativeTextInputOptions options;
    options.text                      = "first";
    UIViewController       *presenter = RootController();
    UINavigationController *retired   = OpenDialog(options, [this](const sky::NativeTextInputEvent &event) {
        RecordDialogEvent(event);
        if (event.type != sky::NativeTextInputEventType::CHANGED) {
            sky::NativeTextInputOptions next;
            next.text       = "second";
            reopenAttempted = true;
            reopenAccepted  = platform->ShowTextInputDialog(next, RecordDialog());
        }
    });
    ASSERT_NE(retired, nil);
    UITextField                                 *retiredField    = DialogField(retired);
    UIPresentationController                    *oldPresentation = retired.presentationController;
    id<UIAdaptivePresentationControllerDelegate> oldDelegate     = oldPresentation.delegate;
    ASSERT_NE(retiredField, nil);
    ASSERT_NE(oldDelegate, nil);
    ASSERT_TRUE(InvokeDialogAction(retired.topViewController.navigationItem.leftBarButtonItem));
    const bool oldPresenterHeld = presenter.presentedViewController == retired;
    bool       exit             = false;
    platform->PollEvent(exit, sky::EventPollMode::NON_BLOCKING);
    if (oldPresenterHeld) {
        EXPECT_FALSE(reopenAttempted);
    }
    ASSERT_TRUE(PumpUntil([this] { return reopenAttempted; }));
    ASSERT_TRUE(reopenAccepted);
    UINavigationController *navigation = WaitForDialog(presenter);
    ASSERT_NE(navigation, nil);
    ASSERT_NE(navigation, retired);
    ASSERT_EQ(DialogTerminalCount(), 1U);
    retiredField.text = @"late change";
    [retiredField sendActionsForControlEvents:UIControlEventEditingChanged];
    EXPECT_TRUE(InvokeDialogAction(retired.topViewController.navigationItem.rightBarButtonItem));
    EXPECT_TRUE(InvokeDialogAction(retired.topViewController.navigationItem.leftBarButtonItem));
    ASSERT_TRUE([oldDelegate respondsToSelector:@selector(presentationControllerDidDismiss:)]);
    [oldDelegate presentationControllerDidDismiss:oldPresentation];
    EXPECT_EQ(DialogEventCount(sky::NativeTextInputEventType::CHANGED), 0U);
    EXPECT_EQ(DialogTerminalCount(), 1U);
    EXPECT_EQ(presenter.presentedViewController, navigation);
    UITextField *field = DialogField(navigation);
    ASSERT_NE(field, nil);
    field.text = @"current";
    [field sendActionsForControlEvents:UIControlEventEditingChanged];
    ASSERT_TRUE(InvokeDialogAction(navigation.topViewController.navigationItem.rightBarButtonItem));
    const size_t terminalBeforeLateCallback = DialogTerminalCount();
    [oldDelegate presentationControllerDidDismiss:oldPresentation];
    EXPECT_TRUE(InvokeDialogAction(retired.topViewController.navigationItem.rightBarButtonItem));
    EXPECT_EQ(DialogTerminalCount(), terminalBeforeLateCallback);
    const bool newPresenterHeld = presenter.presentedViewController == navigation;
    platform->PollEvent(exit, sky::EventPollMode::NON_BLOCKING);
    if (newPresenterHeld) {
        EXPECT_EQ(DialogTerminalCount(), 1U);
    }
    platform->HideTextInputDialog();
    EXPECT_EQ(DialogTerminalCount(), 2U);
    ASSERT_TRUE(PumpUntil([this, presenter] { return DialogTerminalCount() == 2 && presenter.presentedViewController == nil; }));
    EXPECT_EQ(dialogEvents.back().type, sky::NativeTextInputEventType::CONFIRMED);
    EXPECT_EQ(dialogEvents.back().text, "current");
    EXPECT_TRUE(callbacksOnOwner);
}

TEST_F(IOSNativePlatformTest, DialogChangedCallbackCanConfirmAndReopenAfterNativeRetirement)
{
    UIViewController           *presenter = RootController();
    sky::NativeTextInputOptions options;
    options.text                    = "initial";
    UINavigationController *retired = OpenDialog(options, [this, presenter](const sky::NativeTextInputEvent &event) {
        RecordDialogEvent(event);
        if (event.type == sky::NativeTextInputEventType::CHANGED) {
            UIViewController *presented = presenter.presentedViewController;
            ASSERT_TRUE([presented isKindOfClass:[UINavigationController class]]);
            UINavigationController *navigation = static_cast<UINavigationController *>(presented);
            nativeConfirmAccepted              = InvokeDialogAction(navigation.topViewController.navigationItem.rightBarButtonItem);
            bool exit                          = false;
            platform->PollEvent(exit, sky::EventPollMode::NON_BLOCKING);
            EXPECT_EQ(DialogTerminalCount(), 0U);
        } else {
            sky::NativeTextInputOptions next;
            next.text       = "reopened";
            reopenAttempted = true;
            reopenAccepted  = platform->ShowTextInputDialog(next, RecordDialog());
        }
    });
    ASSERT_NE(retired, nil);
    UITextField *field = DialogField(retired);
    ASSERT_NE(field, nil);
    field.text = @"changed";
    [field sendActionsForControlEvents:UIControlEventEditingChanged];
    ASSERT_TRUE(nativeConfirmAccepted);
    EXPECT_EQ(DialogEventCount(sky::NativeTextInputEventType::CHANGED), 1U);
    EXPECT_EQ(DialogTerminalCount(), 0U);
    EXPECT_FALSE(reopenAttempted);
    ASSERT_TRUE(PumpUntil([this] { return reopenAttempted; }));
    ASSERT_TRUE(reopenAccepted);
    ASSERT_EQ(DialogTerminalCount(), 1U);
    EXPECT_EQ(dialogEvents.back().type, sky::NativeTextInputEventType::CONFIRMED);
    EXPECT_EQ(dialogEvents.back().text, "changed");
    UINavigationController *navigation = WaitForDialog(presenter);
    ASSERT_NE(navigation, nil);
    ASSERT_NE(navigation, retired);
    ASSERT_TRUE(InvokeDialogAction(navigation.topViewController.navigationItem.leftBarButtonItem));
    ASSERT_TRUE(PumpUntil([this, presenter] { return DialogTerminalCount() == 2 && presenter.presentedViewController == nil; }));
    EXPECT_EQ(dialogEvents.back().type, sky::NativeTextInputEventType::CANCELLED);
    EXPECT_TRUE(callbacksOnOwner);
}

TEST_F(IOSNativePlatformTest, DialogBackgroundCancellationRejectsReentryUntilSceneActivation)
{
    sky::NativeTextInputOptions options;
    options.text                       = "background";
    UIViewController       *presenter  = RootController();
    UINavigationController *navigation = OpenDialog(options, [this](const sky::NativeTextInputEvent &event) {
        RecordDialogEvent(event);
        if (event.type != sky::NativeTextInputEventType::CHANGED) {
            reopenAttempted = true;
            reopenAccepted  = platform->ShowTextInputDialog({}, RecordDialog());
        }
    });
    ASSERT_NE(navigation, nil);
    [scene.delegate sceneWillResignActive:scene];
    EXPECT_FALSE(platform->IsFrameActive());
    EXPECT_TRUE(reopenAttempted);
    EXPECT_FALSE(reopenAccepted);
    ASSERT_EQ(DialogTerminalCount(), 1U);
    EXPECT_EQ(dialogEvents.back().type, sky::NativeTextInputEventType::CANCELLED);
    ASSERT_TRUE(PumpUntil([presenter] { return presenter.presentedViewController == nil; }));
    [scene.delegate sceneWillEnterForeground:scene];
    EXPECT_FALSE(platform->ShowTextInputDialog({}, RecordDialog()));
    [scene.delegate sceneDidBecomeActive:scene];
    UINavigationController *next = OpenDialog({});
    ASSERT_NE(next, nil);
    ASSERT_TRUE(InvokeDialogAction(next.topViewController.navigationItem.leftBarButtonItem));
    ASSERT_TRUE(PumpUntil([this, presenter] { return DialogTerminalCount() == 2 && presenter.presentedViewController == nil; }));
    EXPECT_TRUE(callbacksOnOwner);
}

TEST_F(IOSNativePlatformTest, DialogSceneDisconnectCancelsAndReconnectCanPresentAgain)
{
    const sky::WindowID     id         = window->GetWinId();
    UINavigationController *navigation = OpenDialog({});
    ASSERT_NE(navigation, nil);
    [scene.delegate sceneDidDisconnect:scene];
    EXPECT_FALSE(platform->IsFrameActive());
    EXPECT_EQ(window->GetNativeHandle(), nullptr);
    ASSERT_EQ(DialogTerminalCount(), 1U);
    EXPECT_EQ(dialogEvents.back().type, sky::NativeTextInputEventType::CANCELLED);
    EXPECT_FALSE(platform->ShowTextInputDialog({}, RecordDialog()));
    ASSERT_TRUE(sky::ios::ConnectScene((__bridge void *)scene));
    EXPECT_EQ(window->GetWinId(), id);
    EXPECT_FALSE(platform->ShowTextInputDialog({}, RecordDialog()));
    [scene.delegate sceneDidBecomeActive:scene];
    UIViewController       *presenter = RootController();
    UINavigationController *next      = OpenDialog({});
    ASSERT_NE(next, nil);
    ASSERT_TRUE(InvokeDialogAction(next.topViewController.navigationItem.leftBarButtonItem));
    ASSERT_TRUE(PumpUntil([this, presenter] { return DialogTerminalCount() == 2 && presenter.presentedViewController == nil; }));
    EXPECT_TRUE(callbacksOnOwner);
}

TEST_F(IOSNativePlatformTest, DialogMainWindowCloseCancelsBeforeReentryAndNotifiesOnce)
{
    WindowCapture           capture(window.get());
    UINavigationController *navigation = OpenDialog({}, [this](const sky::NativeTextInputEvent &event) {
        RecordDialogEvent(event);
        if (event.type != sky::NativeTextInputEventType::CHANGED) {
            reopenAttempted = true;
            reopenAccepted  = platform->ShowTextInputDialog({}, RecordDialog());
        }
    });
    ASSERT_NE(navigation, nil);
    window->Close();
    window->Close();
    EXPECT_TRUE(reopenAttempted);
    EXPECT_FALSE(reopenAccepted);
    EXPECT_FALSE(platform->IsFrameActive());
    EXPECT_EQ(platform->GetMainWinHandle(), nullptr);
    ASSERT_EQ(DialogTerminalCount(), 1U);
    EXPECT_EQ(dialogEvents.back().type, sky::NativeTextInputEventType::CANCELLED);
    EXPECT_EQ(capture.closeCount, 1U);
    window = std::make_unique<sky::IOSWindow>();
    ASSERT_TRUE(window->Init({}));
    sky::ios::SetSceneActive((__bridge void *)scene, true);
    UIViewController       *presenter = RootController();
    UINavigationController *next      = OpenDialog({});
    ASSERT_NE(next, nil);
    ASSERT_TRUE(InvokeDialogAction(next.topViewController.navigationItem.leftBarButtonItem));
    ASSERT_TRUE(PumpUntil([this, presenter] { return DialogTerminalCount() == 2 && presenter.presentedViewController == nil; }));
    EXPECT_TRUE(callbacksOnOwner);
}

TEST_F(IOSNativePlatformTest, DialogCompletionMayDestroyTheClosingWindow)
{
    const sky::WindowID     id           = window->GetWinId();
    CAMetalLayer           *layer        = (__bridge CAMetalLayer *)window->GetNativeHandle();
    UIView                 *retainedView = static_cast<UIView *>(layer.delegate);
    UINavigationController *navigation   = OpenDialog({}, [this](const sky::NativeTextInputEvent &event) {
        RecordDialogEvent(event);
        if (event.type != sky::NativeTextInputEventType::CHANGED) {
            window.reset();
        }
    });
    ASSERT_NE(navigation, nil);
    window->Close();
    EXPECT_EQ(window, nullptr);
    EXPECT_EQ(platform->GetMainWinHandle(), nullptr);
    EXPECT_EQ(sky::NativeWindowManager::Get()->GetWindowByID(id), nullptr);
    EXPECT_FALSE(platform->IsFrameActive());
    EXPECT_EQ(DialogTerminalCount(), 1U);
    [retainedView layoutSubviews];
    EXPECT_TRUE(callbacksOnOwner);
}

TEST_F(IOSNativePlatformTest, DialogCompletionMayDestroyTheWindowDuringSceneDetach)
{
    const sky::WindowID     id           = window->GetWinId();
    CAMetalLayer           *layer        = (__bridge CAMetalLayer *)window->GetNativeHandle();
    UIView                 *retainedView = static_cast<UIView *>(layer.delegate);
    UINavigationController *navigation   = OpenDialog({}, [this](const sky::NativeTextInputEvent &event) {
        RecordDialogEvent(event);
        if (event.type != sky::NativeTextInputEventType::CHANGED) {
            window.reset();
        }
    });
    ASSERT_NE(navigation, nil);
    [scene.delegate sceneDidDisconnect:scene];
    EXPECT_EQ(window, nullptr);
    EXPECT_EQ(platform->GetMainWinHandle(), nullptr);
    EXPECT_EQ(sky::NativeWindowManager::Get()->GetWindowByID(id), nullptr);
    EXPECT_FALSE(platform->IsFrameActive());
    EXPECT_EQ(DialogTerminalCount(), 1U);
    [retainedView layoutSubviews];
    EXPECT_TRUE(callbacksOnOwner);
}

TEST_F(IOSNativePlatformTest, DialogRejectsInvalidOptionsWithoutPoisoningTheNextRequest)
{
    UIViewController           *presenter = RootController();
    sky::NativeTextInputOptions invalid;
    invalid.multiline = true;
    invalid.password  = true;
    EXPECT_FALSE(platform->ShowTextInputDialog(invalid, RecordDialog()));
    invalid           = {};
    invalid.text      = "over";
    invalid.maxLength = 3;
    EXPECT_FALSE(platform->ShowTextInputDialog(invalid, RecordDialog()));
    invalid      = {};
    invalid.text = "\xED\xA0\x80";
    EXPECT_FALSE(platform->ShowTextInputDialog(invalid, RecordDialog()));
    EXPECT_EQ(presenter.presentedViewController, nil);
    EXPECT_TRUE(dialogEvents.empty());
    UINavigationController *navigation = OpenDialog({});
    ASSERT_NE(navigation, nil);
    ASSERT_TRUE(InvokeDialogAction(navigation.topViewController.navigationItem.leftBarButtonItem));
    ASSERT_TRUE(PumpUntil([this, presenter] { return DialogTerminalCount() == 1 && presenter.presentedViewController == nil; }));
    EXPECT_TRUE(callbacksOnOwner);
}

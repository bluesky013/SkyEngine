// Native Cocoa contract tests. These use AppKit windows and synthetic input,
// never a Metal device or drawable. A logged-in WindowServer session is required.

#include "CocoaWindow.h"
#include "MacosPlatform.h"

#include <core/event/Event.h>
#include <framework/platform/PlatformBase.h>
#include <framework/window/NativeWindowManager.h>
#include <gtest/gtest.h>

#include <cmath>
#include <memory>

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

@interface                              SkyFrameworkTestEvent : NSEvent
@property (nonatomic, assign) NSPoint   testLocation;
@property (nonatomic, assign) NSWindow *testWindow;
@property (nonatomic, assign) CGFloat   testScroll;
@property (nonatomic, assign) BOOL      testPrecise;
@property (nonatomic, assign) CGFloat   testDeltaX;
@property (nonatomic, assign) CGFloat   testDeltaY;
@end

@implementation SkyFrameworkTestEvent
- (NSPoint)locationInWindow
{
    return _testLocation;
}
- (NSWindow *)window
{
    return _testWindow;
}
- (CGFloat)scrollingDeltaY
{
    return _testScroll;
}
- (BOOL)hasPreciseScrollingDeltas
{
    return _testPrecise;
}
- (CGFloat)deltaX
{
    return _testDeltaX;
}
- (CGFloat)deltaY
{
    return _testDeltaY;
}
- (NSInteger)clickCount
{
    return 2;
}
@end

namespace {

    class MouseRecorder : public sky::IMouseEvent {
    public:
        MouseRecorder()
        {
            sky::Event<sky::IMouseEvent>::Connect(this);
        }
        ~MouseRecorder() override
        {
            sky::Event<sky::IMouseEvent>::DisConnect(this);
        }
        void OnMouseWheel(const sky::MouseWheelEvent &event) override
        {
            wheel = event;
            ++wheelCount;
            ++eventCount;
        }
        void OnMouseMotion(const sky::MouseMotionEvent &event) override
        {
            motion = event;
            ++eventCount;
        }
        void OnMouseButtonDown(const sky::MouseButtonEvent &event) override
        {
            button = event;
            ++eventCount;
        }
        void OnMouseButtonUp(const sky::MouseButtonEvent &event) override
        {
            button = event;
            ++eventCount;
        }

        sky::MouseWheelEvent  wheel      = {};
        sky::MouseMotionEvent motion     = {};
        sky::MouseButtonEvent button     = {};
        int                   wheelCount = 0;
        int                   eventCount = 0;
    };

    class KeyboardRecorder : public sky::IKeyboardEvent {
    public:
        KeyboardRecorder()
        {
            sky::Event<sky::IKeyboardEvent>::Connect(this);
        }
        ~KeyboardRecorder()
        {
            sky::Event<sky::IKeyboardEvent>::DisConnect(this);
        }
        void OnKeyDown(const sky::KeyboardEvent &) override
        {
            ++eventCount;
        }
        void OnKeyUp(const sky::KeyboardEvent &) override
        {
            ++eventCount;
        }
        void OnTextInput(sky::WindowID, const char *) override
        {
            ++eventCount;
        }

        int eventCount = 0;
    };

    class WindowRecorder : public sky::IWindowEvent {
    public:
        explicit WindowRecorder(const sky::NativeWindow *window)
        {
            sky::Event<sky::IWindowEvent>::Connect(window, this);
        }
        ~WindowRecorder() override
        {
            sky::Event<sky::IWindowEvent>::DisConnect(this);
        }
        void OnWindowResize(const sky::WindowResizeEvent &event) override
        {
            resize = event;
            ++resizeCount;
        }
        void OnWindowClose(const sky::NativeWindow *window) override
        {
            ++closeCount;
            hadLiveHandleAtClose = window->GetNativeHandle() != nullptr;
            if (deleteOnClose != nullptr) {
                deleteOnClose->reset();
            }
        }

        sky::WindowResizeEvent             resize               = {};
        int                                resizeCount          = 0;
        int                                closeCount           = 0;
        bool                               hadLiveHandleAtClose = false;
        std::unique_ptr<sky::CocoaWindow> *deleteOnClose        = nullptr;
    };

    class CocoaPlatformTest : public ::testing::Test {
    protected:
        void SetUp() override
        {
            ASSERT_TRUE([NSThread isMainThread]);
            // Avoid initializing AppKit on machines without an interactive user.
            CFDictionaryRef session = CGSessionCopyCurrentDictionary();
            if (session == nullptr) {
                GTEST_SKIP() << "Cocoa tests require a logged-in WindowServer session";
            }
            CFRelease(session);
            pool = [[NSAutoreleasePool alloc] init];
            ASSERT_TRUE(sky::Platform::Get()->Init({}));
            if ([NSScreen screens].count == 0) {
                GTEST_SKIP() << "Cocoa tests require an available display";
            }
            window = std::make_unique<sky::CocoaWindow>();
            ASSERT_TRUE(window->Init({360, 240, "CocoaContract", "Cocoa Contract", nullptr}));
            [Native() orderOut:nil];
        }
        void TearDown() override
        {
            window.reset();
            if (pool != nil) {
                sky::Platform::Get()->Shutdown();
                [pool drain];
                pool = nil;
            }
        }
        NSWindow *Native() const
        {
            return static_cast<NSWindow *>(window->GetNSWindow());
        }
        SkyFrameworkTestEvent *Input(CGFloat x, CGFloat y) const
        {
            auto *event        = [[[SkyFrameworkTestEvent alloc] init] autorelease];
            event.testLocation = NSMakePoint(x, y);
            event.testWindow   = Native();
            return event;
        }

        NSAutoreleasePool                *pool = nil;
        std::unique_ptr<sky::CocoaWindow> window;
    };

    TEST_F(CocoaPlatformTest, ResizeKeepsClientPixelSizeAndLayerInSync)
    {
        WindowRecorder recorder(window.get());
        NSView        *view = Native().contentView;
        [view setFrameSize:NSMakeSize(210, 140)];
        const NSSize backing = [view convertRectToBacking:view.bounds].size;
        EXPECT_EQ(window->GetWidth(), static_cast<uint32_t>(backing.width));
        EXPECT_EQ(window->GetHeight(), static_cast<uint32_t>(backing.height));
        EXPECT_GT(recorder.resizeCount, 0);
        EXPECT_EQ(recorder.resize.winID, window->GetWinId());
        EXPECT_EQ(recorder.resize.width, window->GetWidth());
        CAMetalLayer *layer = static_cast<CAMetalLayer *>(window->GetNativeHandle());
        EXPECT_DOUBLE_EQ(layer.drawableSize.width, backing.width);
        EXPECT_DOUBLE_EQ(layer.drawableSize.height, backing.height);
    }

    TEST_F(CocoaPlatformTest, FractionalWheelHasClientPixelCoordinates)
    {
        MouseRecorder recorder;
        auto         *event = Input(30.0, 20.0);
        event.testPrecise   = YES;
        event.testScroll    = 0.125;
        NSView *view        = Native().contentView;
        [view scrollWheel:event];
        const NSPoint backing = [view convertPointToBacking:NSMakePoint(30.0, NSHeight(view.bounds) - 20.0)];
        ASSERT_EQ(recorder.wheelCount, 1);
        EXPECT_EQ(recorder.wheel.winID, window->GetWinId());
        EXPECT_EQ(recorder.wheel.x, static_cast<int32_t>(backing.x));
        EXPECT_EQ(recorder.wheel.y, static_cast<int32_t>(backing.y));
        EXPECT_FLOAT_EQ(recorder.wheel.delta, 0.125f * window->GetDpiScale());
        event.testPrecise = NO;
        event.testScroll  = -1.0;
        [view scrollWheel:event];
        EXPECT_FLOAT_EQ(recorder.wheel.delta, -120.0f);
    }

    TEST_F(CocoaPlatformTest, MotionAndButtonsUseClientBackingPixels)
    {
        MouseRecorder recorder;
        auto         *event = Input(30.0, 20.0);
        event.testDeltaX    = 1.25;
        event.testDeltaY    = 0.5;
        NSView *view        = Native().contentView;
        [view mouseMoved:event];
        [view mouseDown:event];
        const NSPoint backing = [view convertPointToBacking:NSMakePoint(30.0, NSHeight(view.bounds) - 20.0)];
        const NSSize  delta   = [view convertSizeToBacking:NSMakeSize(1.25, -0.5)];
        EXPECT_EQ(recorder.motion.x, static_cast<int32_t>(backing.x));
        EXPECT_EQ(recorder.motion.y, static_cast<int32_t>(backing.y));
        EXPECT_EQ(recorder.motion.relX, static_cast<int32_t>(std::lround(delta.width)));
        EXPECT_EQ(recorder.motion.relY, static_cast<int32_t>(std::lround(delta.height)));
        EXPECT_EQ(recorder.button.x, recorder.motion.x);
        EXPECT_EQ(recorder.button.y, recorder.motion.y);
        EXPECT_EQ(recorder.button.clicks, 2u);
    }

    TEST_F(CocoaPlatformTest, MainWindowTitleAndDesktopPositionAreLive)
    {
        EXPECT_TRUE(window->IsMainWindow());
        EXPECT_EQ(sky::NativeWindowManager::Get()->GetMainWindow(), window.get());
        window->SetTitle("World - modified");
        EXPECT_STREQ(Native().title.UTF8String, "World - modified");
        window->SetPosition(120, 160);
        int32_t x = 0;
        int32_t y = 0;
        ASSERT_TRUE(window->GetPosition(x, y));
        EXPECT_NEAR(x, 120, 1);
        EXPECT_NEAR(y, 160, 1);
        EXPECT_TRUE(window->GetGlobalCursorPosition(x, y));
        window->SetCursor(sky::StandardCursor::Text);
        EXPECT_EQ([NSCursor currentCursor], [NSCursor IBeamCursor]);
        window->SetPointerCapture(true);
        window->SetPointerCapture(false);
    }

    TEST_F(CocoaPlatformTest, TextCandidateUsesFramebufferCaretRectangle)
    {
        [Native() makeKeyAndOrderFront:nil];
        NSView<NSTextInputClient> *view = static_cast<NSView<NSTextInputClient> *>(Native().contentView);
        [Native() makeFirstResponder:view];
        sky::Platform::Get()->StartTextInput();
        sky::Platform::Get()->SetTextInputRect(40, 60, 20, 30);
        NSRect caret          = [view convertRectFromBacking:NSMakeRect(40, 60, 20, 30)];
        caret.origin.y        = NSHeight(view.bounds) - NSMaxY(caret);
        const NSRect expected = [Native() convertRectToScreen:[view convertRect:caret toView:nil]];
        const NSRect actual   = [view firstRectForCharacterRange:NSMakeRange(0, 0) actualRange:nullptr];
        EXPECT_DOUBLE_EQ(actual.origin.x, expected.origin.x);
        EXPECT_DOUBLE_EQ(actual.origin.y, expected.origin.y);
        EXPECT_DOUBLE_EQ(actual.size.width, expected.size.width);
        EXPECT_DOUBLE_EQ(actual.size.height, expected.size.height);
        sky::Platform::Get()->StopTextInput();
    }

    TEST_F(CocoaPlatformTest, CloseNotificationPrecedesReentrantOwnerDestruction)
    {
        WindowRecorder recorder(window.get());
        window->SetPointerCapture(true);
        recorder.deleteOnClose = &window;
        NSWindow *native       = [Native() retain];
        [native performClose:nil];
        EXPECT_EQ(recorder.closeCount, 1);
        EXPECT_TRUE(recorder.hadLiveHandleAtClose);
        EXPECT_EQ(window, nullptr);
        EXPECT_EQ(sky::NativeWindowManager::Get()->GetMainWindow(), nullptr);
        bool exit = false;
        sky::Platform::Get()->PoolEvent(exit);
        EXPECT_TRUE(exit);
        [native release];
    }

    TEST_F(CocoaPlatformTest, SecondaryCloseDoesNotQuitMainWindow)
    {
        auto secondary = std::make_unique<sky::CocoaWindow>();
        ASSERT_TRUE(secondary->Init({200, 160, "CocoaSecondary", "Secondary", nullptr}));
        EXPECT_FALSE(secondary->IsMainWindow());
        WindowRecorder recorder(secondary.get());
        [static_cast<NSWindow *>(secondary->GetNSWindow()) performClose:nil];
        EXPECT_EQ(recorder.closeCount, 1);
        EXPECT_TRUE(recorder.hadLiveHandleAtClose);
        bool exit = false;
        sky::Platform::Get()->PoolEvent(exit);
        EXPECT_FALSE(exit);
        secondary.reset();
        EXPECT_EQ(sky::NativeWindowManager::Get()->GetMainWindow(), window.get());
    }

    TEST_F(CocoaPlatformTest, AppQuitUsesNormalLoopExit)
    {
        WindowRecorder            recorder(window.get());
        id<NSApplicationDelegate> delegate = [NSApp delegate];
        ASSERT_TRUE([delegate respondsToSelector:@selector(applicationShouldTerminate:)]);
        EXPECT_EQ([delegate applicationShouldTerminate:NSApp], NSTerminateCancel);
        EXPECT_EQ(recorder.closeCount, 1);
        bool exit = false;
        sky::Platform::Get()->PoolEvent(exit);
        EXPECT_TRUE(exit);
    }

    TEST_F(CocoaPlatformTest, RetainedViewIgnoresNativeInputAfterOwnerDestruction)
    {
        NSView<NSTextInputClient> *view  = [static_cast<NSView<NSTextInputClient> *>(Native().contentView) retain];
        auto                      *mouse = Input(30.0, 20.0);
        mouse.testScroll                 = 0.125;
        mouse.testPrecise                = YES;
        NSEvent *key                     = [NSEvent keyEventWithType:NSEventTypeKeyUp
                                        location:NSZeroPoint
                                   modifierFlags:0
                                       timestamp:0
                                    windowNumber:Native().windowNumber
                                         context:nil
                                      characters:@"a"
                     charactersIgnoringModifiers:@"a"
                                       isARepeat:NO
                                         keyCode:0];
        [view setMarkedText:@"pending" selectedRange:NSMakeRange(0, 0) replacementRange:NSMakeRange(NSNotFound, 0)];
        ASSERT_TRUE([view hasMarkedText]);
        MouseRecorder    mouseRecorder;
        KeyboardRecorder keyboardRecorder;
        window.reset();
        EXPECT_FALSE([view hasMarkedText]);

        // AppKit/IME can retain the responder after the engine owner is gone.
        // Exercise its native entry points with the delayed events themselves.
        [view keyDown:key];
        [view keyUp:key];
        [view insertText:@"late commit" replacementRange:NSMakeRange(NSNotFound, 0)];
        [view setMarkedText:@"late preedit" selectedRange:NSMakeRange(0, 0) replacementRange:NSMakeRange(NSNotFound, 0)];
        [view mouseDown:mouse];
        [view mouseUp:mouse];
        [view mouseMoved:mouse];
        [view scrollWheel:mouse];
        EXPECT_EQ(mouseRecorder.eventCount, 0);
        EXPECT_EQ(keyboardRecorder.eventCount, 0);
        EXPECT_FALSE([view hasMarkedText]);
        [view release];
    }

    TEST_F(CocoaPlatformTest, CommandsReportChildFailureAndBundleFileSystemExists)
    {
        std::string output;
        EXPECT_TRUE(sky::Platform::Get()->RunCmd("printf cocoa-contract", output));
        EXPECT_EQ(output, "cocoa-contract");
        EXPECT_FALSE(sky::Platform::Get()->RunCmd("exit 7", output));
        auto fs = sky::Platform::Get()->GetBundleFileSystem();
        ASSERT_NE(fs, nullptr);
        EXPECT_EQ(fs->GetPath().GetStr(), sky::Platform::Get()->GetBundlePath());
        EXPECT_FALSE(sky::Platform::Get()->GetEnvVariable("PATH").empty());
    }

} // namespace

//
// Native UIKit window implementation (iOS). Built with ARC.
//

#include "IOSWindow.h"
#include "IOSPlatform.h"

#include <core/event/Event.h>
#include <core/logger/Logger.h>
#include <framework/platform/PlatformBase.h>
#include <framework/window/IWindowEvent.h>
#include <framework/window/NativeWindowManager.h>

#import <QuartzCore/CAMetalLayer.h>
#import <UIKit/UIKit.h>

#include <atomic>

static const char *TAG = "IOSWindow";

namespace sky {

    bool EnsureUIApplication();

    namespace {
        std::atomic<uint32_t> gNextWindowId{0};
    }

} // namespace sky

// UIKit classes live in the global namespace; they forward to sky::IOSWindow.

@interface SkyIOSView : UIView <UIKeyInput>
@property(nonatomic, assign) sky::IOSWindow *owner;
@end

@implementation SkyIOSView

+ (Class)layerClass
{
    return [CAMetalLayer class];
}

- (BOOL)canBecomeFirstResponder
{
    return YES;
}

- (CAMetalLayer *)metalLayer
{
    return (CAMetalLayer *)self.layer;
}

- (void)broadcastResize
{
    const CGSize size = self.bounds.size;
    const CGFloat scale = self.window.screen.nativeScale > 0 ? self.window.screen.nativeScale : 1.0;
    sky::WindowResizeEvent event = {};
    event.winID  = _owner->GetWinId();
    event.width  = static_cast<uint32_t>(size.width * scale);
    event.height = static_cast<uint32_t>(size.height * scale);
    if (event.width != 0 && event.height != 0) {
        sky::Event<sky::IWindowEvent>::BroadCast(_owner, &sky::IWindowEvent::OnWindowResize, event);
    }
}

- (void)didMoveToWindow
{
    [super didMoveToWindow];
    self.metalLayer.contentsScale = self.window.screen.nativeScale;
    [self broadcastResize];
    [self becomeFirstResponder];
}

- (void)layoutSubviews
{
    [super layoutSubviews];
    [self broadcastResize];
}

#pragma mark - Touch

- (void)dispatchMotion:(UIEvent *)event
{
    UITouch *touch = [[event allTouches] anyObject];
    if (touch == nil) {
        return;
    }
    const CGPoint p = [touch locationInView:self];
    sky::MouseMotionEvent motion = {};
    motion.winID = _owner->GetWinId();
    motion.x = static_cast<int32_t>(p.x);
    motion.y = static_cast<int32_t>(p.y);
    sky::Event<sky::IMouseEvent>::BroadCast(&sky::IMouseEvent::OnMouseMotion, motion);
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    UITouch *touch = [touches anyObject];
    const CGPoint p = [touch locationInView:self];
    sky::MouseButtonEvent button = {};
    button.winID  = _owner->GetWinId();
    button.button = sky::MouseButtonType::LEFT;
    button.clicks = static_cast<uint32_t>(touch.tapCount);
    button.x = static_cast<int32_t>(p.x);
    button.y = static_cast<int32_t>(p.y);
    sky::Event<sky::IMouseEvent>::BroadCast(&sky::IMouseEvent::OnMouseButtonDown, button);
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [self dispatchMotion:event];
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    UITouch *touch = [touches anyObject];
    const CGPoint p = [touch locationInView:self];
    sky::MouseButtonEvent button = {};
    button.winID  = _owner->GetWinId();
    button.button = sky::MouseButtonType::LEFT;
    button.clicks = static_cast<uint32_t>(touch.tapCount);
    button.x = static_cast<int32_t>(p.x);
    button.y = static_cast<int32_t>(p.y);
    sky::Event<sky::IMouseEvent>::BroadCast(&sky::IMouseEvent::OnMouseButtonUp, button);
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [self touchesEnded:touches withEvent:event];
}

#pragma mark - UIKeyInput (soft keyboard)

- (BOOL)hasText
{
    return YES;
}

- (void)insertText:(NSString *)text
{
    if (text.length == 0) {
        return;
    }
    sky::Event<sky::IKeyboardEvent>::BroadCast(&sky::IKeyboardEvent::OnTextInput,
                                               _owner->GetWinId(), text.UTF8String);
}

- (void)deleteBackward
{
    sky::KeyboardEvent keyEvent = {};
    keyEvent.winID    = _owner->GetWinId();
    keyEvent.scanCode = sky::ScanCode::KEY_BACKSPACE;
    sky::Event<sky::IKeyboardEvent>::BroadCast(&sky::IKeyboardEvent::OnKeyDown, keyEvent);
}

@end

namespace sky {

    IOSWindow::~IOSWindow()
    {
        if (window != nullptr) {
            NativeWindowManager::Get()->UnRegister(this);
            auto *uiWindow = (__bridge_transfer UIWindow *)window;
            [uiWindow setHidden:YES];
            window     = nullptr;
            controller = nullptr;
            metalLayer = nullptr;
            winHandle  = nullptr;
            (void)uiWindow; // released by ARC at scope end
        }
    }

    bool IOSWindow::Init(const Descriptor &desc)
    {
        EnsureUIApplication();
        descriptor = desc;

        const CGRect frame = CGRectMake(0, 0, desc.width, desc.height);
        UIWindow *uiWindow = [[UIWindow alloc] initWithFrame:frame];
        SkyIOSView *view = [[SkyIOSView alloc] initWithFrame:frame];
        view.owner = this;
        view.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

        UIViewController *rootController = [[UIViewController alloc] init];
        rootController.view = view;
        uiWindow.rootViewController = rootController;
        [uiWindow makeKeyAndVisible];

        window     = (__bridge_retained void *)uiWindow;
        controller = (__bridge void *)rootController; // retained by uiWindow
        metalLayer = (__bridge void *)view.metalLayer;
        winHandle  = metalLayer;
        scale      = static_cast<float>(uiWindow.screen.nativeScale);

        SetID(static_cast<WindowID>(++gNextWindowId));
        NativeWindowManager::Get()->Register(this);

        if (auto *iosPlatform = dynamic_cast<IOSPlatform *>(Platform::Get()->GetImpl())) {
            iosPlatform->SetMainWindow(metalLayer);
        }
        return true;
    }

    void *IOSWindow::GetNativeHandle() const
    {
        return metalLayer;
    }

    float IOSWindow::GetDpiScale() const
    {
        if (window == nullptr) {
            return scale;
        }
        const CGFloat nativeScale = ((__bridge UIWindow *)window).screen.nativeScale;
        return nativeScale > 0 ? static_cast<float>(nativeScale) : scale;
    }

    void IOSWindow::SetTitle(const std::string &title)
    {
        (void)title; // iOS has no window title
    }

    NativeWindow *NativeWindow::Create(const Descriptor &des)
    {
        NativeWindow *window = new IOSWindow();
        if (!window->Init(des)) {
            delete window;
            window = nullptr;
        }
        return window;
    }

} // namespace sky

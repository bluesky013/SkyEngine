//
// Native UIKit window implementation (iOS). Built with ARC.
//

#include "IOSWindow.h"
#include "IOSPlatform.h"
#include "IOSPlatformState.h"

#include <core/event/Event.h>
#include <framework/platform/PlatformBase.h>
#include <framework/window/IWindowEvent.h>
#include <framework/window/NativeWindowManager.h>

#import <QuartzCore/CAMetalLayer.h>
#import <UIKit/UIKit.h>

#include <algorithm>
#include <atomic>

namespace sky {

    namespace {
        std::atomic<uint32_t> gNextWindowId{0};

        IOSPlatform *GetIOSPlatform()
        {
            Platform *platform = Platform::Get();
            return platform->GetImpl() != nullptr && platform->GetType() == PlatformType::IOS ? static_cast<IOSPlatform *>(platform->GetImpl())
                                                                                              : nullptr;
        }
    } // namespace

} // namespace sky

// UITextView supplies UIKit's full marked-text/IME implementation. Only
// committed text leaves this adapter; composition remains owned by UIKit.
@interface                                    SkyIOSTextView : UITextView
@property (nonatomic, assign) sky::IOSWindow *owner;
@end

@implementation SkyIOSTextView
- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent *)event
{
    (void)point;
    (void)event;
    return NO; // The invisible IME anchor must not intercept engine touches.
}

- (void)deleteBackward
{
    if (self.markedTextRange == nil && _owner != nullptr) {
        sky::KeyboardEvent event = {};
        event.winID              = _owner->GetWinId();
        event.scanCode           = sky::ScanCode::KEY_BACKSPACE;
        sky::Event<sky::IKeyboardEvent>::BroadCast(&sky::IKeyboardEvent::OnKeyDown, event);
        sky::Event<sky::IKeyboardEvent>::BroadCast(&sky::IKeyboardEvent::OnKeyUp, event);
    }
    [super deleteBackward];
}
@end

@interface SkyIOSView : UIView <UITextViewDelegate> {
    sky::ios::PrimaryTouch touchState;
}
@property (nonatomic, assign) sky::IOSWindow *owner;
@property (nonatomic, strong) SkyIOSTextView *textView;
- (CAMetalLayer *)metalLayer;
- (void)cancelPrimaryTouch;
- (void)setTextInputEnabled:(BOOL)enabled;
- (void)setTextInputFrame:(CGRect)frame;
@end

@implementation SkyIOSView

+ (Class)layerClass
{
    return [CAMetalLayer class];
}

- (CAMetalLayer *)metalLayer
{
    return (CAMetalLayer *)self.layer;
}

- (double)nativeScale
{
    return sky::ios::ValidScale(self.window.windowScene.screen.nativeScale);
}

- (void)broadcastResize
{
    if (_owner == nullptr) {
        return;
    }
    const double   scale          = [self nativeScale];
    const uint32_t width          = sky::ios::ToFramebufferExtent(self.bounds.size.width, scale);
    const uint32_t height         = sky::ios::ToFramebufferExtent(self.bounds.size.height, scale);
    self.contentScaleFactor       = scale;
    self.metalLayer.contentsScale = scale;
    self.metalLayer.drawableSize  = CGSizeMake(width, height);
    _owner->NotifyFramebufferSize(width, height, static_cast<float>(scale));
}

- (void)didMoveToWindow
{
    [super didMoveToWindow];
    [self broadcastResize];
}

- (void)layoutSubviews
{
    [super layoutSubviews];
    [self broadcastResize];
}

- (sky::ios::PixelPoint)pixelPosition:(UITouch *)touch
{
    const CGPoint point = [touch locationInView:self];
    const double  scale = [self nativeScale];
    return {sky::ios::ToFramebufferCoordinate(point.x, scale), sky::ios::ToFramebufferCoordinate(point.y, scale)};
}

- (void)dispatchButton:(sky::ios::PixelPoint)point clicks:(uint32_t)clicks down:(BOOL)down
{
    if (_owner == nullptr) {
        return;
    }
    sky::MouseButtonEvent event = {};
    event.winID                 = _owner->GetWinId();
    event.button                = sky::MouseButtonType::LEFT;
    event.clicks                = clicks;
    event.x                     = point.x;
    event.y                     = point.y;
    if (down) {
        sky::Event<sky::IMouseEvent>::BroadCast(&sky::IMouseEvent::OnMouseButtonDown, event);
    } else {
        sky::Event<sky::IMouseEvent>::BroadCast(&sky::IMouseEvent::OnMouseButtonUp, event);
    }
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    (void)event;
    // The first accepted touch remains the mouse pointer until it ends. Later
    // fingers cannot move/release it or take over a drag without a new begin.
    UITouch *first = nil;
    for (UITouch *touch in touches) {
        if (first == nil || touch.timestamp < first.timestamp) {
            first = touch;
        }
    }
    if (first != nil) {
        const auto point = [self pixelPosition:first];
        if (touchState.Begin(reinterpret_cast<uintptr_t>((__bridge void *)first), point)) {
            [self dispatchButton:point clicks:static_cast<uint32_t>(first.tapCount) down:YES];
        }
    }
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    (void)event;
    if (_owner == nullptr) {
        return;
    }
    for (UITouch *touch in touches) {
        const auto           point = [self pixelPosition:touch];
        sky::ios::PixelPoint delta;
        if (touchState.Move(reinterpret_cast<uintptr_t>((__bridge void *)touch), point, delta)) {
            sky::MouseMotionEvent motion = {};
            motion.winID                 = _owner->GetWinId();
            motion.x                     = point.x;
            motion.y                     = point.y;
            motion.relX                  = delta.x;
            motion.relY                  = delta.y;
            sky::Event<sky::IMouseEvent>::BroadCast(&sky::IMouseEvent::OnMouseMotion, motion);
        }
    }
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    (void)event;
    for (UITouch *touch in touches) {
        const auto point = [self pixelPosition:touch];
        if (touchState.End(reinterpret_cast<uintptr_t>((__bridge void *)touch), point)) {
            [self dispatchButton:point clicks:static_cast<uint32_t>(touch.tapCount) down:NO];
        }
    }
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    (void)event;
    for (UITouch *touch in touches) {
        const auto point = [self pixelPosition:touch];
        if (touchState.End(reinterpret_cast<uintptr_t>((__bridge void *)touch), point)) {
            [self dispatchButton:point clicks:0 down:NO];
        }
    }
}

- (void)cancelPrimaryTouch
{
    if (touchState.Cancel()) {
        [self dispatchButton:touchState.GetPosition() clicks:0 down:NO];
    }
}

- (void)ensureTextView
{
    if (_textView != nil) {
        return;
    }
    _textView                                   = [[SkyIOSTextView alloc] initWithFrame:CGRectMake(0, 0, 1, 1)];
    _textView.owner                             = _owner;
    _textView.delegate                          = self;
    _textView.backgroundColor                   = [UIColor clearColor];
    _textView.textColor                         = [UIColor clearColor];
    _textView.tintColor                         = [UIColor clearColor];
    _textView.scrollEnabled                     = NO;
    _textView.textContainerInset                = UIEdgeInsetsZero;
    _textView.textContainer.lineFragmentPadding = 0;
    _textView.autocorrectionType                = UITextAutocorrectionTypeNo;
    _textView.autocapitalizationType            = UITextAutocapitalizationTypeNone;
    [self addSubview:_textView];
}

- (void)setTextInputEnabled:(BOOL)enabled
{
    if (enabled) {
        [self ensureTextView];
        [_textView becomeFirstResponder];
    } else {
        // Discard an unfinished composition when focus/input ends.
        _textView.delegate = nil;
        [_textView unmarkText];
        _textView.text = @"";
        [_textView resignFirstResponder];
        _textView.delegate = self;
    }
}

- (void)setTextInputFrame:(CGRect)frame
{
    [self ensureTextView];
    _textView.frame = frame;
}

- (void)textViewDidChange:(UITextView *)textView
{
    if (_owner == nullptr || textView.markedTextRange != nil || textView.text.length == 0) {
        return;
    }
    NSString *committed = [textView.text copy];
    textView.text       = @"";
    const char *utf8    = committed.UTF8String;
    if (utf8 != nullptr) {
        sky::Event<sky::IKeyboardEvent>::BroadCast(&sky::IKeyboardEvent::OnTextInput, _owner->GetWinId(), utf8);
    }
}

@end

namespace sky {

    IOSWindow::~IOSWindow()
    {
        Close();
        // Close listeners may trigger destruction during an earlier surface/focus
        // notification, before the caller has finished retiring its native view.
        ReleaseSurface();
    }

    bool IOSWindow::Init(const Descriptor &desc)
    {
        IOSPlatform *platform = GetIOSPlatform();
        if (closed || winID != INVALID_WIN_ID || platform == nullptr || platform->GetWindowScene() == nullptr ||
            !platform->RegisterMainWindow(this)) {
            return false;
        }
        descriptor = desc;
        SetID(static_cast<WindowID>(++gNextWindowId));
        NativeWindowManager::Get()->Register(this);
        return AttachScene(platform->GetWindowScene());
    }

    bool IOSWindow::AttachScene(void *handle)
    {
        IOSPlatform *platform = GetIOSPlatform();
        if (closed || handle == nullptr || platform == nullptr || platform->GetWindowScene() != handle || platform->GetMainWindow() != this) {
            return false;
        }
        if (window != nullptr) {
            return GetWindowScene() == handle;
        }

        UIWindowScene *scene    = (__bridge UIWindowScene *)handle;
        UIWindow      *uiWindow = [[UIWindow alloc] initWithWindowScene:scene];
        if (uiWindow == nil) {
            return false;
        }
        uiWindow.frame                   = scene.coordinateSpace.bounds;
        SkyIOSView *view                 = [[SkyIOSView alloc] initWithFrame:uiWindow.bounds];
        view.multipleTouchEnabled        = YES;
        view.autoresizingMask            = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
        UIViewController *rootController = [[UIViewController alloc] init];
        rootController.view              = view;
        uiWindow.rootViewController      = rootController;

        // Construction/layout callbacks stay inert until all C++ and native
        // ownership is established. Reconnection must not register a new ID.
        window    = (__bridge_retained void *)uiWindow;
        winHandle = (__bridge void *)view.metalLayer;
        scale     = static_cast<float>(ios::ValidScale(scene.screen.nativeScale));
        platform->SetWindowSurface(this, winHandle);
        view.owner          = this;
        view.textView.owner = this;

        NSNotificationCenter *center      = [NSNotificationCenter defaultCenter];
        id                    keyToken    = [center addObserverForName:UIWindowDidBecomeKeyNotification
                                          object:uiWindow
                                           queue:nil
                                      usingBlock:^(NSNotification *) {
                                          this->SetFocused(true);
                                      }];
        id                    resignToken = [center addObserverForName:UIWindowDidResignKeyNotification
                                             object:uiWindow
                                              queue:nil
                                         usingBlock:^(NSNotification *) {
                                             this->SetFocused(false);
                                         }];
        focusObservers                    = (__bridge_retained void *)@[ keyToken, resignToken ];
        [uiWindow makeKeyAndVisible];
        if (view.owner != this) {
            return false;
        }
        [view broadcastResize];
        if (view.owner != this) {
            return false;
        }
        Event<IWindowEvent>::BroadCast(this, &IWindowEvent::OnSurfaceChanged, static_cast<const NativeWindow *>(this));
        return true;
    }

    void IOSWindow::RemoveFocusObservers()
    {
        if (focusObservers == nullptr) {
            return;
        }
        NSArray *tokens = (__bridge_transfer NSArray *)focusObservers;
        focusObservers  = nullptr;
        for (id token in tokens) {
            [[NSNotificationCenter defaultCenter] removeObserver:token];
        }
    }

    void IOSWindow::ReleaseSurface()
    {
        RemoveFocusObservers();
        if (window == nullptr) {
            return;
        }
        UIWindow *uiWindow     = (__bridge_transfer UIWindow *)window;
        window                 = nullptr;
        winHandle              = nullptr;
        focused                = false;
        SkyIOSView *view       = static_cast<SkyIOSView *>(uiWindow.rootViewController.view);
        view.owner             = nullptr;
        view.textView.owner    = nullptr;
        view.textView.delegate = nil;
        [view.textView unmarkText];
        view.textView.text = @"";
        [view.textView resignFirstResponder];
        [uiWindow setHidden:YES];
        uiWindow.rootViewController = nil;
    }

    void IOSWindow::DetachScene()
    {
        if (window == nullptr) {
            return;
        }
        UIWindow   *uiWindow = (__bridge UIWindow *)window;
        SkyIOSView *view     = static_cast<SkyIOSView *>(uiWindow.rootViewController.view);
        const bool  hadFocus = focused;
        focused              = false;
        winHandle            = nullptr;
        RemoveFocusObservers();
        if (IOSPlatform *platform = GetIOSPlatform()) {
            platform->SetWindowSurface(this, nullptr);
        }
        [view cancelPrimaryTouch];
        if (view.owner != this) {
            return;
        }
        [view setTextInputEnabled:NO];
        if (hadFocus) {
            Event<IWindowEvent>::BroadCast(this, &IWindowEvent::OnFocusChanged, false);
            if (view.owner != this) {
                return;
            }
        }
        ReleaseSurface();
        // Surface loss is not a close request. Keep the engine window registered
        // so the next connected scene can attach to this same object and ID.
        Event<IWindowEvent>::BroadCast(this, &IWindowEvent::OnSurfaceChanged, static_cast<const NativeWindow *>(this));
    }

    void IOSWindow::Close()
    {
        if (closed || winID == INVALID_WIN_ID) {
            return;
        }
        UIWindow   *uiWindow = (__bridge UIWindow *)window;
        SkyIOSView *view     = static_cast<SkyIOSView *>(uiWindow.rootViewController.view);
        const bool  hadFocus = focused;
        closed               = true;
        focused              = false;
        textInputEnabled     = false;
        winHandle            = nullptr;
        if (IOSPlatform *platform = GetIOSPlatform()) {
            platform->ClearMainWindow(this);
        }
        NativeWindowManager::Get()->UnRegister(this);
        RemoveFocusObservers();
        if (view != nil) {
            [view cancelPrimaryTouch];
            if (view.owner != this) {
                return;
            }
            [view setTextInputEnabled:NO];
            if (hadFocus) {
                Event<IWindowEvent>::BroadCast(this, &IWindowEvent::OnFocusChanged, false);
                if (view.owner != this) {
                    return;
                }
            }
            Event<IWindowEvent>::BroadCast(this, &IWindowEvent::OnSurfaceChanged, static_cast<const NativeWindow *>(this));
            if (view.owner != this) {
                return;
            }
            ReleaseSurface();
        }
        // OnWindowClose may destroy the owning unique_ptr. All native state and
        // registry ownership is detached before this last notification.
        Event<IWindowEvent>::BroadCast(this, &IWindowEvent::OnWindowClose, static_cast<const NativeWindow *>(this));
    }

    void IOSWindow::SetFocused(bool value)
    {
        if (window == nullptr) {
            return;
        }
        if (value) {
            const IOSPlatform *platform = GetIOSPlatform();
            value                       = !closed && ((__bridge UIWindow *)window).isKeyWindow && platform != nullptr && platform->IsFrameActive();
        }
        if (focused == value) {
            return;
        }
        focused                     = value;
        const bool  enableTextInput = focused && textInputEnabled;
        SkyIOSView *view            = static_cast<SkyIOSView *>(((__bridge UIWindow *)window).rootViewController.view);
        if (!focused) {
            [view cancelPrimaryTouch];
            if (view.owner != this) {
                return;
            }
        }
        [view setTextInputEnabled:enableTextInput];
        if (view.owner != this) {
            return;
        }
        Event<IWindowEvent>::BroadCast(this, &IWindowEvent::OnFocusChanged, value);
    }

    void IOSWindow::SetTextInputEnabled(bool enabled)
    {
        textInputEnabled = enabled;
        if (window == nullptr) {
            return;
        }
        SkyIOSView *view = static_cast<SkyIOSView *>(((__bridge UIWindow *)window).rootViewController.view);
        [view setTextInputEnabled:enabled && focused && !closed];
    }

    void IOSWindow::SetTextInputRect(int32_t x, int32_t y, int32_t width, int32_t height)
    {
        if (window == nullptr) {
            return;
        }
        const double nativeScale = ios::ValidScale(GetDpiScale());
        SkyIOSView  *view        = static_cast<SkyIOSView *>(((__bridge UIWindow *)window).rootViewController.view);
        [view setTextInputFrame:CGRectMake(x / nativeScale, y / nativeScale, std::max(width, 1) / nativeScale, std::max(height, 1) / nativeScale)];
    }

    void IOSWindow::NotifyFramebufferSize(uint32_t width, uint32_t height, float nativeScale)
    {
        if (closed) {
            return;
        }
        const bool changed = descriptor.width != width || descriptor.height != height || scale != nativeScale;
        descriptor.width   = width;
        descriptor.height  = height;
        scale              = nativeScale;
        if (changed) {
            const WindowResizeEvent event{GetWinId(), width, height};
            Event<IWindowEvent>::BroadCast(this, &IWindowEvent::OnWindowResize, event);
        }
    }

    void *IOSWindow::GetNativeHandle() const
    {
        return winHandle;
    }

    void *IOSWindow::GetWindowScene() const
    {
        return window != nullptr ? (__bridge void *)((__bridge UIWindow *)window).windowScene : nullptr;
    }

    float IOSWindow::GetDpiScale() const
    {
        return window != nullptr ? static_cast<float>(ios::ValidScale(((__bridge UIWindow *)window).windowScene.screen.nativeScale)) : scale;
    }

    bool IOSWindow::IsMainWindow() const
    {
        const IOSPlatform *platform = GetIOSPlatform();
        return platform != nullptr && platform->GetMainWindow() == this;
    }

    void IOSWindow::SetTitle(const std::string &title)
    {
        descriptor.titleName = title;
    }

    NativeWindow *NativeWindow::Create(const Descriptor &desc)
    {
        NativeWindow *window = new IOSWindow();
        if (!window->Init(desc)) {
            delete window;
            window = nullptr;
        }
        return window;
    }

} // namespace sky

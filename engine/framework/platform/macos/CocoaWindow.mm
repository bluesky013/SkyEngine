//
// Native Cocoa window implementation (no SDL). The content view is layer-backed
// by a CAMetalLayer (via makeBackingLayer) so the RHI can render into it
// directly; GetNativeHandle() hands out that layer.
//

#include "CocoaWindow.h"
#include "MacosPlatform.h"

#include <core/environment/Once.h>
#include <core/event/Event.h>
#include <core/logger/Logger.h>
#include <framework/platform/PlatformBase.h>
#include <framework/window/IWindowEvent.h>
#include <framework/window/NativeWindowManager.h>

#include <cmath>

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

static const char *TAG = "CocoaWindow";

namespace sky {

    // Ensures NSApplication exists and is activated; safe to call repeatedly.
    bool EnsureNSApplication()
    {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        if (Once::Get()->Run("CocoaApplicationLaunch")) {
            [NSApp finishLaunching];
        }

        if ([NSApp mainMenu] == nil) {
            // minimal app menu so Cmd+Q terminates the app
            auto *menuBar = [[NSMenu alloc] init];
            auto *appItem = [[NSMenuItem alloc] init];
            auto *appMenu = [[NSMenu alloc] init];
            [appMenu addItem:[[[NSMenuItem alloc] initWithTitle:@"Quit" action:@selector(terminate:) keyEquivalent:@"q"] autorelease]];
            [appItem setSubmenu:appMenu];
            [menuBar addItem:appItem];
            [NSApp setMainMenu:menuBar];
            [appMenu release];
            [appItem release];
            [menuBar release];
        }
        return true;
    }

    namespace {

        MacosPlatform *GetMacosPlatform()
        {
            auto *impl = Platform::Get()->GetImpl();
            return impl != nullptr && impl->GetType() == PlatformType::MacOS ? static_cast<MacosPlatform *>(impl) : nullptr;
        }

        // A single primary-display scale defines a reversible desktop coordinate
        // plane, including mixed-DPI displays. Client coordinates use each view's
        // own backing conversion instead.
        NSPoint DesktopPointToPixels(NSPoint point)
        {
            NSScreen     *primary = [[NSScreen screens] firstObject];
            const CGFloat factor  = primary != nil ? primary.backingScaleFactor : 1.0;
            const NSRect  frame   = primary != nil ? primary.frame : NSZeroRect;
            return NSMakePoint((point.x - NSMinX(frame)) * factor, (NSMaxY(frame) - point.y) * factor);
        }

        NSPoint DesktopPixelsToPoint(int32_t x, int32_t y)
        {
            NSScreen     *primary = [[NSScreen screens] firstObject];
            const CGFloat factor  = primary != nil ? primary.backingScaleFactor : 1.0;
            const NSRect  frame   = primary != nil ? primary.frame : NSZeroRect;
            return NSMakePoint(NSMinX(frame) + x / factor, NSMaxY(frame) - y / factor);
        }

        NSCursor *CursorFor(StandardCursor cursor)
        {
            switch (cursor) {
            case StandardCursor::ResizeHorizontal: return [NSCursor resizeLeftRightCursor];
            case StandardCursor::ResizeVertical: return [NSCursor resizeUpDownCursor];
            case StandardCursor::ResizeAll: return [NSCursor closedHandCursor];
            case StandardCursor::Text: return [NSCursor IBeamCursor];
            case StandardCursor::Hand: return [NSCursor pointingHandCursor];
            default: return [NSCursor arrowCursor];
            }
        }

        // macOS virtual key codes (Carbon-style, US ANSI layout) -> ScanCode
        ScanCode FromMacKeyCode(uint16_t keyCode)
        {
            switch (keyCode) {
            case 0x00: return ScanCode::KEY_A;
            case 0x0B: return ScanCode::KEY_B;
            case 0x08: return ScanCode::KEY_C;
            case 0x02: return ScanCode::KEY_D;
            case 0x0E: return ScanCode::KEY_E;
            case 0x03: return ScanCode::KEY_F;
            case 0x05: return ScanCode::KEY_G;
            case 0x04: return ScanCode::KEY_H;
            case 0x22: return ScanCode::KEY_I;
            case 0x26: return ScanCode::KEY_J;
            case 0x28: return ScanCode::KEY_K;
            case 0x25: return ScanCode::KEY_L;
            case 0x2E: return ScanCode::KEY_M;
            case 0x2D: return ScanCode::KEY_N;
            case 0x1F: return ScanCode::KEY_O;
            case 0x23: return ScanCode::KEY_P;
            case 0x0C: return ScanCode::KEY_Q;
            case 0x0F: return ScanCode::KEY_R;
            case 0x01: return ScanCode::KEY_S;
            case 0x11: return ScanCode::KEY_T;
            case 0x20: return ScanCode::KEY_U;
            case 0x09: return ScanCode::KEY_V;
            case 0x0D: return ScanCode::KEY_W;
            case 0x07: return ScanCode::KEY_X;
            case 0x10: return ScanCode::KEY_Y;
            case 0x06: return ScanCode::KEY_Z;
            case 0x12: return ScanCode::KEY_1;
            case 0x13: return ScanCode::KEY_2;
            case 0x14: return ScanCode::KEY_3;
            case 0x15: return ScanCode::KEY_4;
            case 0x17: return ScanCode::KEY_5;
            case 0x16: return ScanCode::KEY_6;
            case 0x1A: return ScanCode::KEY_7;
            case 0x1C: return ScanCode::KEY_8;
            case 0x19: return ScanCode::KEY_9;
            case 0x1D: return ScanCode::KEY_0;
            case 0x24: return ScanCode::KEY_RETURN;
            case 0x35: return ScanCode::KEY_ESCAPE;
            case 0x33: return ScanCode::KEY_BACKSPACE;
            case 0x30: return ScanCode::KEY_TAB;
            case 0x31: return ScanCode::KEY_SPACE;
            case 0x1B: return ScanCode::KEY_MINUS;
            case 0x18: return ScanCode::KEY_EQUALS;
            case 0x21: return ScanCode::KEY_LEFTBRACKET;
            case 0x1E: return ScanCode::KEY_RIGHTBRACKET;
            case 0x2A: return ScanCode::KEY_BACKSLASH;
            case 0x29: return ScanCode::KEY_SEMICOLON;
            case 0x27: return ScanCode::KEY_APOSTROPHE;
            case 0x32: return ScanCode::KEY_GRAVE;
            case 0x2B: return ScanCode::KEY_COMMA;
            case 0x2F: return ScanCode::KEY_PERIOD;
            case 0x2C: return ScanCode::KEY_SLASH;
            case 0x39: return ScanCode::KEY_CAPSLOCK;
            case 0x7A: return ScanCode::KEY_F1;
            case 0x78: return ScanCode::KEY_F2;
            case 0x63: return ScanCode::KEY_F3;
            case 0x76: return ScanCode::KEY_F4;
            case 0x60: return ScanCode::KEY_F5;
            case 0x61: return ScanCode::KEY_F6;
            case 0x62: return ScanCode::KEY_F7;
            case 0x64: return ScanCode::KEY_F8;
            case 0x65: return ScanCode::KEY_F9;
            case 0x6D: return ScanCode::KEY_F10;
            case 0x67: return ScanCode::KEY_F11;
            case 0x6F: return ScanCode::KEY_F12;
            case 0x72: return ScanCode::KEY_INSERT;
            case 0x73: return ScanCode::KEY_HOME;
            case 0x74: return ScanCode::KEY_PAGEUP;
            case 0x75: return ScanCode::KEY_DELETE;
            case 0x77: return ScanCode::KEY_END;
            case 0x79: return ScanCode::KEY_PAGEDOWN;
            case 0x7C: return ScanCode::KEY_RIGHT;
            case 0x7B: return ScanCode::KEY_LEFT;
            case 0x7D: return ScanCode::KEY_DOWN;
            case 0x7E: return ScanCode::KEY_UP;
            case 0x47: return ScanCode::KEY_NUMLOCKCLEAR;
            case 0x4B: return ScanCode::KEY_KP_DIVIDE;
            case 0x43: return ScanCode::KEY_KP_MULTIPLY;
            case 0x4E: return ScanCode::KEY_KP_MINUS;
            case 0x45: return ScanCode::KEY_KP_PLUS;
            case 0x4C: return ScanCode::KEY_KP_ENTER;
            case 0x41: return ScanCode::KEY_KP_PERIOD;
            case 0x52: return ScanCode::KEY_KP_0;
            case 0x53: return ScanCode::KEY_KP_1;
            case 0x54: return ScanCode::KEY_KP_2;
            case 0x55: return ScanCode::KEY_KP_3;
            case 0x56: return ScanCode::KEY_KP_4;
            case 0x57: return ScanCode::KEY_KP_5;
            case 0x58: return ScanCode::KEY_KP_6;
            case 0x59: return ScanCode::KEY_KP_7;
            case 0x5B: return ScanCode::KEY_KP_8;
            case 0x5C: return ScanCode::KEY_KP_9;
            default: return ScanCode::KEY_NUM;
            }
        }

        // AppKit reports combined modifier flags without left/right; both bits
        // are set so chord checks (e.g. CTRL) behave the same as on Win32.
        KeyModFlags CurrentModifiers(NSEventModifierFlags flags)
        {
            KeyModFlags mod;
            if (flags & NSEventModifierFlagShift) {
                mod |= KeyMod::LEFT_SHIFT;
                mod |= KeyMod::RIGHT_SHIFT;
            }
            if (flags & NSEventModifierFlagControl) {
                mod |= KeyMod::LEFT_CTRL;
                mod |= KeyMod::RIGHT_CTRL;
            }
            if (flags & NSEventModifierFlagOption) {
                mod |= KeyMod::LEFT_ALT;
                mod |= KeyMod::RIGHT_ALT;
            }
            if (flags & NSEventModifierFlagCommand) {
                mod |= KeyMod::LEFT_GUI;
                mod |= KeyMod::RIGHT_GUI;
            }
            if (flags & NSEventModifierFlagCapsLock) {
                mod |= KeyMod::CAPS;
            }
            return mod;
        }

    } // namespace
} // namespace sky

// Obj-C view/delegate classes live in the global namespace (Obj-C has no
// namespaces); they forward to the owning sky::CocoaWindow.

@interface SkyCocoaView : NSView <NSTextInputClient> {
    NSString *_markedText; // in-flight IME composition (preedit)
}
@property (nonatomic, assign) sky::CocoaWindow *owner;
@property (nonatomic, retain) NSCursor         *requestedCursor;
@property (nonatomic, assign) NSRect            textInputRect;
@property (nonatomic, assign) BOOL              hasTextInputRect;
@property (nonatomic, assign) BOOL              textInputDisabled;
- (void)broadcastResize;
- (void)dispatchMotion:(NSEvent *)event;
- (void)detachOwner;
@end

@implementation SkyCocoaView

- (BOOL)acceptsFirstResponder
{
    return YES;
}
- (BOOL)isOpaque
{
    return YES;
}

- (void)detachOwner
{
    // Deactivation may synchronously finish composition. Make the native view
    // inert first, since it can outlive the C++ owner through AppKit/IME retains.
    _owner             = nullptr;
    _textInputDisabled = YES;
    [self unmarkText];
    [self.inputContext discardMarkedText];
    [self.inputContext deactivate];
}

- (void)dealloc
{
    [_markedText release];
    [_requestedCursor release];
    [super dealloc];
}

- (CALayer *)makeBackingLayer
{
    return [CAMetalLayer layer];
}

- (CAMetalLayer *)metalLayer
{
    return (CAMetalLayer *)self.layer;
}

- (void)updateTrackingAreas
{
    [super updateTrackingAreas];
    for (NSTrackingArea *area in [self trackingAreas]) {
        [self removeTrackingArea:area];
    }
    auto *area = [[NSTrackingArea alloc] initWithRect:self.bounds
                                              options:NSTrackingMouseMoved | NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect
                                                owner:self
                                             userInfo:nil];
    [self addTrackingArea:area];
    [area release];
}

- (void)resetCursorRects
{
    [super resetCursorRects];
    [self addCursorRect:self.bounds cursor:_requestedCursor != nil ? _requestedCursor : [NSCursor arrowCursor]];
}

- (NSPoint)flippedViewPoint:(NSEvent *)event
{
    NSPoint location = [event locationInWindow];
    if (event.window != nil && event.window != self.window) {
        location = [self.window convertPointFromScreen:[event.window convertPointToScreen:location]];
    }
    NSPoint p = [self convertPoint:location fromView:nil];
    p.y       = self.bounds.size.height - p.y; // engine convention: top-left origin
    return [self convertPointToBacking:p];
}

- (void)broadcastResize
{
    if (_owner == nullptr) {
        return;
    }
    const NSSize   backing = [self convertRectToBacking:self.bounds].size;
    const uint32_t w       = static_cast<uint32_t>(backing.width);
    const uint32_t h       = static_cast<uint32_t>(backing.height);
    if (w == 0 || h == 0) {
        return;
    }
    [self metalLayer].drawableSize = backing;
    _owner->NotifyResized(w, h);
    if (_owner->GetWinId() == sky::INVALID_WIN_ID) {
        return;
    }
    sky::WindowResizeEvent event = {};
    event.winID                  = _owner->GetWinId();
    event.width                  = w;
    event.height                 = h;
    sky::Event<sky::IWindowEvent>::BroadCast(_owner, &sky::IWindowEvent::OnWindowResize, event);
}

- (void)setFrameSize:(NSSize)newSize
{
    [super setFrameSize:newSize];
    [self metalLayer].contentsScale = self.window.backingScaleFactor;
    [self broadcastResize];
}

- (void)viewDidChangeBackingProperties
{
    [super viewDidChangeBackingProperties];
    [self metalLayer].contentsScale = self.window.backingScaleFactor;
    [self broadcastResize];
}

- (void)keyDown:(NSEvent *)event
{
    [[self retain] autorelease];
    if (_owner == nullptr) {
        return;
    }
    sky::KeyboardEvent keyEvent = {};
    keyEvent.winID              = _owner->GetWinId();
    keyEvent.scanCode           = sky::FromMacKeyCode(event.keyCode);
    keyEvent.mod                = sky::CurrentModifiers(event.modifierFlags);
    sky::Event<sky::IKeyboardEvent>::BroadCast(&sky::IKeyboardEvent::OnKeyDown, keyEvent);

    // Route through the text system so plain text arrives via insertText: and
    // CJK/IME composition works (NSTextInputClient). Command chords are handled
    // by the menu / doCommandBySelector and must not produce text.
    if (_owner != nullptr && !_textInputDisabled) {
        [self interpretKeyEvents:@[ event ]];
    }
}

- (void)keyUp:(NSEvent *)event
{
    if (_owner == nullptr) {
        return;
    }
    sky::KeyboardEvent keyEvent = {};
    keyEvent.winID              = _owner->GetWinId();
    keyEvent.scanCode           = sky::FromMacKeyCode(event.keyCode);
    keyEvent.mod                = sky::CurrentModifiers(event.modifierFlags);
    sky::Event<sky::IKeyboardEvent>::BroadCast(&sky::IKeyboardEvent::OnKeyUp, keyEvent);
}

#pragma mark - NSTextInputClient

- (BOOL)hasMarkedText
{
    return _markedText.length > 0;
}

- (NSRange)markedRange
{
    return _markedText.length > 0 ? NSMakeRange(0, _markedText.length) : NSMakeRange(NSNotFound, 0);
}

- (NSRange)selectedRange
{
    return NSMakeRange(NSNotFound, 0);
}

- (void)setMarkedText:(id)string selectedRange:(NSRange)selectedRange replacementRange:(NSRange)replacementRange
{
    if (_owner == nullptr || _textInputDisabled) {
        return;
    }
    NSString *text = [string isKindOfClass:[NSAttributedString class]] ? [string string] : string;
    [_markedText release];
    _markedText = [text copy];
}

- (void)unmarkText
{
    [_markedText release];
    _markedText = nil;
}

- (NSArray *)validAttributesForMarkedText
{
    return @[];
}

- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range actualRange:(NSRangePointer)actualRange
{
    return nil;
}

- (void)insertText:(id)string replacementRange:(NSRange)replacementRange
{
    if (_owner == nullptr || _textInputDisabled) {
        return;
    }
    [self unmarkText];

    NSString *text = [string isKindOfClass:[NSAttributedString class]] ? [string string] : string;
    if (text.length == 0) {
        return;
    }
    sky::Event<sky::IKeyboardEvent>::BroadCast(&sky::IKeyboardEvent::OnTextInput, _owner->GetWinId(), text.UTF8String);
}

// Special keys (arrows, delete, ...) reach here after interpretKeyEvents; the
// physical key was already broadcast as OnKeyDown, so they are swallowed here.
- (void)doCommandBySelector:(SEL)selector
{
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point
{
    return NSNotFound;
}

// The engine caret is expressed in top-left client framebuffer pixels.
- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actualRange
{
    NSRect caret            = _hasTextInputRect ? [self convertRectFromBacking:_textInputRect] : NSZeroRect;
    caret.origin.y          = NSHeight(self.bounds) - NSMaxY(caret);
    const NSRect windowRect = [self convertRect:caret toView:nil];
    if (actualRange != nullptr) {
        *actualRange = range;
    }
    return [self.window convertRectToScreen:windowRect];
}

- (void)dispatchButton:(NSEvent *)event type:(sky::MouseButtonType)button down:(BOOL)down
{
    if (_owner == nullptr) {
        return;
    }
    const NSPoint         p          = [self flippedViewPoint:event];
    sky::MouseButtonEvent mouseEvent = {};
    mouseEvent.winID                 = _owner->GetWinId();
    mouseEvent.button                = button;
    mouseEvent.clicks                = static_cast<uint32_t>(event.clickCount);
    mouseEvent.x                     = static_cast<int32_t>(p.x);
    mouseEvent.y                     = static_cast<int32_t>(p.y);
    if (down) {
        sky::Event<sky::IMouseEvent>::BroadCast(&sky::IMouseEvent::OnMouseButtonDown, mouseEvent);
    } else {
        sky::Event<sky::IMouseEvent>::BroadCast(&sky::IMouseEvent::OnMouseButtonUp, mouseEvent);
    }
}

- (void)dispatchMotion:(NSEvent *)event
{
    if (_owner == nullptr) {
        return;
    }
    const NSPoint         p           = [self flippedViewPoint:event];
    sky::MouseMotionEvent motionEvent = {};
    motionEvent.winID                 = _owner->GetWinId();
    motionEvent.x                     = static_cast<int32_t>(p.x);
    motionEvent.y                     = static_cast<int32_t>(p.y);
    const NSSize delta                = [self convertSizeToBacking:NSMakeSize(event.deltaX, -event.deltaY)];
    motionEvent.relX                  = static_cast<int32_t>(std::lround(delta.width));
    motionEvent.relY                  = static_cast<int32_t>(std::lround(delta.height));
    sky::Event<sky::IMouseEvent>::BroadCast(&sky::IMouseEvent::OnMouseMotion, motionEvent);
}

- (void)mouseDown:(NSEvent *)event
{
    [self dispatchButton:event type:sky::MouseButtonType::LEFT down:YES];
}
- (void)mouseUp:(NSEvent *)event
{
    [self dispatchButton:event type:sky::MouseButtonType::LEFT down:NO];
}
- (void)rightMouseDown:(NSEvent *)event
{
    [self dispatchButton:event type:sky::MouseButtonType::RIGHT down:YES];
}
- (void)rightMouseUp:(NSEvent *)event
{
    [self dispatchButton:event type:sky::MouseButtonType::RIGHT down:NO];
}
- (void)otherMouseDown:(NSEvent *)event
{
    [self dispatchButton:event type:sky::MouseButtonType::MIDDLE down:YES];
}
- (void)otherMouseUp:(NSEvent *)event
{
    [self dispatchButton:event type:sky::MouseButtonType::MIDDLE down:NO];
}

- (void)mouseMoved:(NSEvent *)event
{
    [self dispatchMotion:event];
}
- (void)mouseDragged:(NSEvent *)event
{
    [self dispatchMotion:event];
}
- (void)rightMouseDragged:(NSEvent *)event
{
    [self dispatchMotion:event];
}
- (void)otherMouseDragged:(NSEvent *)event
{
    [self dispatchMotion:event];
}

- (void)scrollWheel:(NSEvent *)event
{
    if (_owner == nullptr) {
        return;
    }
    sky::MouseWheelEvent wheelEvent = {};
    wheelEvent.winID                = _owner->GetWinId();
    const NSPoint cursor            = [self flippedViewPoint:event];
    wheelEvent.x                    = static_cast<int32_t>(cursor.x);
    wheelEvent.y                    = static_cast<int32_t>(cursor.y);
    // Precise trackpad deltas are points; conventional wheel detents use the
    // same 120-unit convention as Win32. Preserve fractional movement.
    const CGFloat factor = event.hasPreciseScrollingDeltas ? _owner->GetDpiScale() : 120.0;
    wheelEvent.delta     = static_cast<float>(event.scrollingDeltaY * factor);
    sky::Event<sky::IMouseEvent>::BroadCast(&sky::IMouseEvent::OnMouseWheel, wheelEvent);
}

@end

@interface                                      SkyCocoaWindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, assign) sky::CocoaWindow *owner;
@property (nonatomic, assign) BOOL              closing;
@end

@implementation SkyCocoaWindowDelegate

- (void)windowDidBecomeKey:(NSNotification *)notification
{
    sky::Event<sky::IWindowEvent>::BroadCast(_owner, &sky::IWindowEvent::OnFocusChanged, true);
}

- (void)windowDidResignKey:(NSNotification *)notification
{
    // Drop any in-flight IME composition when the window loses focus. Uses
    // respondsToSelector: rather than isKindOfClass: because Framework is linked
    // statically into both the host and its modules, so the Obj-C class may be
    // registered more than once and cross-image class identity is unreliable.
    NSResponder *responder = [[notification object] firstResponder];
    if ([responder respondsToSelector:@selector(unmarkText)]) {
        [(id)responder unmarkText];
    }
    sky::Event<sky::IWindowEvent>::BroadCast(_owner, &sky::IWindowEvent::OnFocusChanged, false);
}

- (void)notifyClosing:(NSWindow *)nativeWindow
{
    if (_closing || _owner == nullptr) {
        return;
    }
    _closing = YES;
    // A listener may destroy the C++ owner and release its window/delegate.
    // Hold the Objective-C objects through AppKit's current event dispatch and
    // do not access the owner after the broadcast.
    [[self retain] autorelease];
    [[nativeWindow retain] autorelease];
    auto *owner = _owner;
    if (auto *platform = sky::GetMacosPlatform()) {
        platform->NotifyWindowClosed(owner->GetNativeHandle());
    }
    sky::Event<sky::IWindowEvent>::BroadCast(owner, &sky::IWindowEvent::OnWindowClose, static_cast<const sky::NativeWindow *>(owner));
}

- (BOOL)windowShouldClose:(NSWindow *)sender
{
    [self notifyClosing:sender];
    return YES;
}

- (void)windowWillClose:(NSNotification *)notification
{
    [self notifyClosing:[notification object]];
}

- (void)windowDidMove:(NSNotification *)notification
{
    (void)notification;
    if (_owner == nullptr) {
        return;
    }
    sky::WindowMoveEvent event = {};
    event.winID                = _owner->GetWinId();
    _owner->GetPosition(event.x, event.y);
    sky::Event<sky::IWindowEvent>::BroadCast(_owner, &sky::IWindowEvent::OnWindowMove, event);
}

@end

namespace sky {

    bool CocoaWindow::Init(const Descriptor &desc)
    {
        EnsureNSApplication();
        descriptor = desc;

        NSScreen     *screen      = [NSScreen mainScreen];
        const CGFloat factor      = screen != nil ? screen.backingScaleFactor : 1.0;
        const NSRect  contentRect = NSMakeRect(0, 0, desc.width / factor, desc.height / factor);
        auto         *view        = [[SkyCocoaView alloc] initWithFrame:contentRect];
        view.owner                = this;
        [view setWantsLayer:YES]; // makes makeBackingLayer provide the CAMetalLayer

        const NSWindowStyleMask style =
            NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
        auto *nsWindow = [[NSWindow alloc] initWithContentRect:contentRect styleMask:style backing:NSBackingStoreBuffered defer:NO];
        [nsWindow setReleasedWhenClosed:NO]; // lifetime owned by this object
        [nsWindow setTitle:[NSString stringWithUTF8String:desc.titleName.c_str()]];
        [nsWindow setContentView:view];
        [view release];
        [nsWindow center];
        [nsWindow setAcceptsMouseMovedEvents:YES];

        auto *windowDelegate = [[SkyCocoaWindowDelegate alloc] init];
        windowDelegate.owner = this;
        [nsWindow setDelegate:windowDelegate]; // not retained by NSWindow

        window                                     = nsWindow;
        delegate                                   = windowDelegate;
        metalLayer                                 = [view metalLayer];
        winHandle                                  = metalLayer;
        scale                                      = static_cast<float>([nsWindow backingScaleFactor]);
        ((CAMetalLayer *)metalLayer).contentsScale = scale;

        SetID(static_cast<WindowID>([nsWindow windowNumber]));
        NativeWindowManager::Get()->Register(this);

        if (auto *macPlatform = GetMacosPlatform()) {
            macPlatform->SetMainWindow(metalLayer);
        }

        [nsWindow makeKeyAndOrderFront:nil];
        [nsWindow makeFirstResponder:view];
        [NSApp activateIgnoringOtherApps:YES];
        [view broadcastResize];
        return true;
    }

    CocoaWindow::~CocoaWindow()
    {
        SetPointerCapture(false);
        if (window != nullptr) {
            NativeWindowManager::Get()->UnRegister(this);
            auto *nsWindow = static_cast<NSWindow *>(window);
            [(SkyCocoaView *)nsWindow.contentView detachOwner];
            [(SkyCocoaWindowDelegate *)delegate setOwner:nullptr];
            if (auto *platform = GetMacosPlatform()) {
                platform->NotifyWindowDestroyed(metalLayer);
            }
            [nsWindow setDelegate:nil];
            [nsWindow close];
            [nsWindow release];
            [static_cast<NSObject *>(delegate) release];
            window     = nullptr;
            delegate   = nullptr;
            metalLayer = nullptr;
            winHandle  = nullptr;
        }
    }

    void *CocoaWindow::GetNativeHandle() const
    {
        return metalLayer;
    }

    float CocoaWindow::GetDpiScale() const
    {
        if (window == nullptr) {
            return scale;
        }
        return static_cast<float>([static_cast<NSWindow *>(window) backingScaleFactor]);
    }

    void CocoaWindow::NotifyResized(uint32_t width, uint32_t height)
    {
        descriptor.width  = width;
        descriptor.height = height;
    }

    bool CocoaWindow::GetPosition(int32_t &x, int32_t &y) const
    {
        if (window == nullptr) {
            return false;
        }
        const NSRect  frame    = static_cast<NSWindow *>(window).frame;
        const NSPoint position = DesktopPointToPixels(NSMakePoint(NSMinX(frame), NSMaxY(frame)));
        x                      = static_cast<int32_t>(std::lround(position.x));
        y                      = static_cast<int32_t>(std::lround(position.y));
        return true;
    }

    void CocoaWindow::SetPosition(int32_t x, int32_t y)
    {
        if (window != nullptr) {
            [static_cast<NSWindow *>(window) setFrameTopLeftPoint:DesktopPixelsToPoint(x, y)];
        }
    }

    bool CocoaWindow::GetGlobalCursorPosition(int32_t &x, int32_t &y) const
    {
        const NSPoint position = DesktopPointToPixels([NSEvent mouseLocation]);
        x                      = static_cast<int32_t>(std::lround(position.x));
        y                      = static_cast<int32_t>(std::lround(position.y));
        return true;
    }

    void CocoaWindow::SetPointerCapture(bool capture)
    {
        if (captureMonitor != nullptr) {
            [NSEvent removeMonitor:static_cast<id>(captureMonitor)];
            [static_cast<id>(captureMonitor) release];
            captureMonitor = nullptr;
        }
        if (!capture || window == nullptr) {
            return;
        }
        auto *view = static_cast<SkyCocoaView *>(static_cast<NSWindow *>(window).contentView);
        // AppKit already tracks native drags outside the originating window.
        // The local monitor additionally routes events crossing another engine
        // window without requiring accessibility permission or a global grab.
        const NSEventMask mask = NSEventMaskMouseMoved | NSEventMaskLeftMouseDragged | NSEventMaskRightMouseDragged | NSEventMaskOtherMouseDragged |
                                 NSEventMaskLeftMouseUp | NSEventMaskRightMouseUp | NSEventMaskOtherMouseUp;
        id monitor     = [NSEvent addLocalMonitorForEventsMatchingMask:mask
                                                           handler:^NSEvent *(NSEvent *event) {
                                                               if (view.owner == nullptr || event.window == view.window) {
                                                                   return event;
                                                               }
                                                               [[view retain] autorelease];
                                                               switch (event.type) {
                                                               case NSEventTypeLeftMouseUp: [view mouseUp:event]; break;
                                                               case NSEventTypeRightMouseUp: [view rightMouseUp:event]; break;
                                                               case NSEventTypeOtherMouseUp: [view otherMouseUp:event]; break;
                                                               default: [view dispatchMotion:event]; break;
                                                               }
                                                               return nil;
                                                           }];
        captureMonitor = [monitor retain];
    }

    void CocoaWindow::SetCursor(StandardCursor cursor)
    {
        if (window != nullptr) {
            auto *nsWindow       = static_cast<NSWindow *>(window);
            auto *view           = static_cast<SkyCocoaView *>(nsWindow.contentView);
            view.requestedCursor = CursorFor(cursor);
            [nsWindow invalidateCursorRectsForView:view];
            [view.requestedCursor set];
        }
    }

    void CocoaWindow::SetTitle(const std::string &title)
    {
        descriptor.titleName = title;
        if (window != nullptr) {
            [static_cast<NSWindow *>(window) setTitle:[NSString stringWithUTF8String:title.c_str()]];
        }
    }

    bool CocoaWindow::IsMainWindow() const
    {
        auto *platform = GetMacosPlatform();
        return platform != nullptr && metalLayer != nullptr && platform->GetMainWinHandle() == metalLayer;
    }

    NativeWindow *NativeWindow::Create(const Descriptor &des)
    {
        NativeWindow *window = new CocoaWindow();
        if (!window->Init(des)) {
            delete window;
            window = nullptr;
        }
        return window;
    }

} // namespace sky

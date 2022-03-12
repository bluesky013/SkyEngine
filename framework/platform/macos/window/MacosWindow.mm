//
// Created by Zach Lee on 2021/11/28.
//

#include <framework/window/NativeWindow.h>
#include <framework/Application.h>
#include <core/platform/Platform.h>
#import <AppKit/AppKit.h>

namespace sky {
    class MacosWindowImpl;
}

@interface MacosViewController : NSViewController {}
@property (nonatomic) sky::MacosWindowImpl* nativeWindow;
- (void)windowWillClose:(NSNotification *)notification;
- (void)windowDidResize:(NSNotification *)notification;
@end

namespace sky {

    class MacosWindowImpl : public NativeWindowImpl {
    public:
        MacosWindowImpl() {}
        virtual ~MacosWindowImpl();

        bool Init(const NativeWindow::Descriptor&);

        void DeInit();

    private:
        bool CreateMacosWindow(const NativeWindow::Descriptor&);

        void* GetNativeHandle() const override { return handle.contentView; };

        void SetEventHandler(IWindowEvent& h) override
        {
            handler = &h;
        }
        NSWindow* handle = nil;
        NSString* title = nil;
        MacosViewController* controller = nullptr;
        IWindowEvent* handler = nullptr;
    };

    MacosWindowImpl::~MacosWindowImpl()
    {
        DeInit();
    }

    bool MacosWindowImpl::Init(const NativeWindow::Descriptor& des)
    {
        if (!CreateMacosWindow(des)) {
            return false;
        }

        return true;
    }

    bool MacosWindowImpl::CreateMacosWindow(const NativeWindow::Descriptor &des)
    {
        NSRect contentRect;
        contentRect.origin.x = 0;
        contentRect.origin.y = 0;
        contentRect.size.width = des.width;
        contentRect.size.height = des.height;

        auto mask = NSWindowStyleMaskTitled|NSWindowStyleMaskMiniaturizable|NSWindowStyleMaskResizable|NSWindowStyleMaskClosable;

        handle = [[NSWindow alloc]
            initWithContentRect:contentRect
            styleMask: mask
            backing: NSBackingStoreBuffered
            defer:NO];

        title = [NSString stringWithCString:des.titleName.c_str() encoding:NSUTF8StringEncoding];

        [handle setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
        [handle makeKeyAndOrderFront:nil];
        [handle setAcceptsMouseMovedEvents:YES];
        [handle setOpaque:YES];

        controller = [MacosViewController alloc];
        [controller init];
        [controller setView : handle.contentView];
        [controller retain];
        controller.nativeWindow = this;

        NSBundle* bundle = [NSBundle bundleWithPath: @"/System/Library/Frameworks/QuartzCore.framework"];
        CALayer* layer = [[bundle classNamed: @"CAMetalLayer"] layer];
        NSView* view = handle.contentView;
        [view setLayer: layer];
        [view setWantsLayer: YES];

        //Used for window close notification
        [[NSNotificationCenter defaultCenter] addObserver: controller
                                                 selector: @selector(windowWillClose:)
                                                     name: NSWindowWillCloseNotification
                                                   object: handle];

        //Used for window resizing notification
        [[NSNotificationCenter defaultCenter] addObserver:controller
                                                 selector:@selector(windowDidResize:)
                                                     name:NSWindowDidResizeNotification
                                                   object:handle];

        handle.contentViewController = controller;
        handle.title = title;

        return true;
    }

    void MacosWindowImpl::DeInit()
    {
        if (handle != nil) {
            [handle release];
            handle = nil;
        }
        title = nil;
        controller = nil;
    }
}

@implementation MacosViewController
- (void)windowWillClose:(NSNotification *)notification
{
    if (_nativeWindow != nullptr) {
        _nativeWindow->DeInit();
    }
}
- (void)windowDidResize:(NSNotification *)notification
{
}
@end

extern "C" SKY_EXPORT sky::NativeWindowImpl* CreateNativeWindow(const sky::NativeWindow::Descriptor& des)
{
    auto impl = new sky::MacosWindowImpl();
    if (!impl->Init(des)) {
        delete impl;
        impl = nullptr;
    }
    return impl;
}
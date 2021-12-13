//
// Created by Zach Lee on 2021/11/28.
//

#include <framework/window/NativeWindow.h>
#include <core/platform/Platform.h>
#import <AppKit/AppKit.h>

@interface MacosViewController : NSViewController {}
- (void)windowWillClose:(NSNotification *)notification;
- (void)windowDidResize:(NSNotification *)notification;
@end

@implementation MacosViewController
- (void)windowWillClose:(NSNotification *)notification
{

}
- (void)windowDidResize:(NSNotification *)notification
{

}
@end

namespace sky {

    class MacosWindowImpl : public NativeWindow::Impl {
    public:
        MacosWindowImpl() {}
        virtual ~MacosWindowImpl();

        bool Init(const NativeWindow::Descriptor&);

    private:
        bool CreateMacosWindow(const NativeWindow::Descriptor&);

        void DeInit();

        void* GetNativeHandle() const override { return handle; };
        NSWindow* handle = nullptr;
        NSString* title = nullptr;
        MacosViewController* controller = nullptr;
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

        auto mask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable;

        handle = [[NSWindow alloc]
            initWithContentRect:contentRect
            styleMask: mask
            backing: NSBackingStoreBuffered
            defer:NO];

        title = [NSString stringWithCString:des.titleName.c_str() encoding:NSUTF8StringEncoding];
        [handle setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
        [handle makeKeyAndOrderFront:nil];

        NSBundle* bundle = [NSBundle bundleWithPath: @"/System/Library/Frameworks/QuartzCore.framework"];
        CALayer* layer = [[bundle classNamed: @"CAMetalLayer"] layer];

        [handle.contentView setLayer: layer];
        [handle.contentView setWantsLayer: YES];

        controller = [MacosViewController alloc];
        [controller init];
        [controller setView : handle.contentView];
        [controller retain];


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
        if (handle != nullptr) {
            [handle release];
            handle = nullptr;
        }
        title = nullptr;
        controller = nullptr;
    }
}

extern "C" SKY_EXPORT sky::NativeWindow::Impl* CreateWindow(const sky::NativeWindow::Descriptor& des)
{
    auto impl = new sky::MacosWindowImpl();
    if (!impl->Init(des)) {
        delete impl;
        impl = nullptr;
    }
    return impl;
}
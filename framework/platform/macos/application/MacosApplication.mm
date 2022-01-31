
//
// Created by Zach Lee on 2021/11/28.
//

#include <framework/Application.h>
#include <core/platform/Platform.h>
#import <AppKit/NSApplication.h>
#import <Cocoa/Cocoa.h>

@interface SkyEngineDelegate : NSObject <NSApplicationDelegate>
@end

@implementation SkyEngineDelegate
-(BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)theApplication
{
    return YES;
}

-(void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    // Make the application a foreground application (else it won't receive keyboard events)
    ProcessSerialNumber psn = {0, kCurrentProcess};
    TransformProcessType(&psn, kProcessTransformToForegroundApplication);
}
@end


namespace sky {

    class MacosApplicationImpl : public ApplicationImpl {
    public:
        MacosApplicationImpl();

        virtual ~MacosApplicationImpl() = default;

        void PumpMessages() override;

        bool IsExit() const override;

        void SetExit() override;

    private:
        bool PumpMessageInternal();

        bool exit = false;
    };

    MacosApplicationImpl::MacosApplicationImpl()
    {
        @autoreleasepool {
            NSApp = [NSApplication sharedApplication];
            [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
            SkyEngineDelegate *delegate = [[SkyEngineDelegate alloc] init];
            [[NSApplication sharedApplication] setDelegate:delegate];

            [NSApp finishLaunching];
        }
    }

    void MacosApplicationImpl::PumpMessages()
    {
        while (PumpMessageInternal());
    }

    bool MacosApplicationImpl::PumpMessageInternal()
    {
        @autoreleasepool
        {
            NSEvent* event = [NSApp nextEventMatchingMask: NSEventMaskAny
                untilDate: [NSDate distantPast]
                inMode: NSDefaultRunLoopMode
                dequeue: YES];

            if (event != nil) {
                [NSApp sendEvent: event];
                return true;
            } else {
                return false;
            }
        }
    }

    bool MacosApplicationImpl::IsExit() const
    {
        return exit;
    }

    void MacosApplicationImpl::SetExit()
    {
        exit = true;
    }
}

extern "C" SKY_EXPORT sky::ApplicationImpl* CreateApplication()
{
    return new sky::MacosApplicationImpl();
}

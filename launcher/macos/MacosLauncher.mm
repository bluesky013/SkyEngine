//
// Created by Zach Lee on 2021/11/28.
//

#include "application/Application.h"
#import <Cocoa/Cocoa.h>
#include <iostream>

@interface SkyEngineApplication : NSApplication
{
}
@end


@interface SkyEngineApplicationDelegate : NSObject<NSApplicationDelegate> {
    @public sky::Application* app;
}

@end

@implementation SkyEngineApplication

@end

@implementation SkyEngineApplicationDelegate
{
}
- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    // Insert code here to initialize your application
}

- (void)applicationWillTerminate:(NSNotification *)aNotification
{
    // Insert code here to tear down your application
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
    return YES;
}
@end

void ProcessCommand(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        auto str = argv[i];
        std::cout << str << std::endl;
    }
}

int main(int argc, char** argv)
{
    ProcessCommand(argc, argv);

    sky::Application app;

    NSAutoreleasePool* autoreleasePool = [[NSAutoreleasePool alloc] init];
    [SkyEngineApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    SkyEngineApplicationDelegate* appDelegate = [[SkyEngineApplicationDelegate alloc] init];
    appDelegate->app = &app;

    [NSApp setDelegate: appDelegate];

    [[NSUserDefaults standardUserDefaults] registerDefaults:
        [[NSDictionary alloc] initWithObjectsAndKeys:
            [NSNumber numberWithBool:FALSE], @"AppleMomentumScrollSupported",
            [NSNumber numberWithBool:FALSE], @"ApplePressAndHoldEnabled",
                nil]];

    [NSApp finishLaunching];
    [autoreleasePool release];

    sky::StartInfo start = {};
    start.appName = "MacosLauncher";
    start.modules = {
        "SampleModule"
    };

    if (app.Init(start)) {
        app.Mainloop();
    }

    app.Shutdown();

    return 0;
}
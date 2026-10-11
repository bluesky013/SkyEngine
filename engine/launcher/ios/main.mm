//
// iOS launcher: UIApplicationMain owns the run loop; the engine frame loop is
// driven by the iOS platform's CADisplayLink (no Application::Mainloop).
//

#include <framework/application/GameApplication.h>
#include <framework/platform/PlatformBase.h>

#include <memory>

#import <UIKit/UIKit.h>

using namespace sky;

namespace {
    int                              gArgc = 0;
    char                           **gArgv = nullptr;
    std::unique_ptr<GameApplication> gApp;
}

@interface SkyLauncherDelegate : UIResponder <UIApplicationDelegate>
@end

@implementation SkyLauncherDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    (void)application;
    (void)launchOptions;

    Platform::Get()->Init({});

    gApp = std::make_unique<GameApplication>();
    if (!gApp->Init(gArgc, gArgv)) {
        return NO;
    }

    // iOS owns the run loop: do not enter Application::Mainloop; the platform's
    // CADisplayLink ticks Application::Loop() each frame.
    gApp->SetExternalFrameLoop(true);
    Platform *platform = Platform::Get();
    platform->SetFrameCallback([]() {
        if (gApp) {
            gApp->Loop();
        }
    });
    platform->StartFrameLoop();
    return YES;
}

- (void)applicationWillTerminate:(UIApplication *)application
{
    (void)application;
    gApp.reset();
}

@end

int main(int argc, char **argv)
{
    gArgc = argc;
    gArgv = argv;
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([SkyLauncherDelegate class]));
    }
}

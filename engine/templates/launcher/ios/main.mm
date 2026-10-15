//
// iOS launcher: the application delegate owns process setup; the scene delegate
// connects UIKit windows before starting the process-owned game application.
//

#include <framework/application/GameApplication.h>
#include <framework/platform/IOSSceneHost.h>
#include <framework/platform/PlatformBase.h>

#include <memory>

#import <UIKit/UIKit.h>

namespace {

    class IOSGameHost {
    public:
        void SetArguments(int argc, char **argv)
        {
            argumentCount = argc;
            arguments     = argv;
        }

        bool InitPlatform(UIApplication *application)
        {
            if (platformAttempted) {
                return platformReady;
            }
            platformAttempted = true;
            platformReady     = sky::Platform::Get()->Init({(__bridge void *)application});
            if (!platformReady) {
                sky::Platform::Get()->Shutdown();
            }
            return platformReady;
        }

        bool Connect(UIWindowScene *scene)
        {
            if (!platformReady || terminated || (game != nullptr && game->IsExit())) {
                return false;
            }
            if (!sky::ios::ConnectScene((__bridge void *)scene)) {
                return false;
            }
            if (gameAttempted) {
                return game != nullptr;
            }

            // Reflection and modules are process-owned, not scene-owned. Never
            // repeat their initialization when UIKit reconnects a scene.
            gameAttempted = true;
            game          = std::make_unique<sky::GameApplication>();
            if (!game->Init(argumentCount, arguments)) {
                Shutdown();
                return false;
            }
            game->SetExternalFrameLoop(true);
            sky::Platform *platform = sky::Platform::Get();
            platform->SetFrameCallback([this]() {
                if (game == nullptr || game->IsExit()) {
                    StopFrames();
                    return;
                }
                game->Loop();
                if (game->IsExit()) {
                    StopFrames();
                }
            });
            platform->StartFrameLoop();
            return true;
        }

        void SetSceneActive(UIWindowScene *scene, bool active)
        {
            if (platformReady && !terminated) {
                sky::ios::SetSceneActive((__bridge void *)scene, active);
            }
        }

        void Disconnect(UIWindowScene *scene)
        {
            if (platformReady && !terminated) {
                sky::ios::DisconnectScene((__bridge void *)scene);
            }
        }

        void Shutdown()
        {
            if (terminated) {
                return;
            }
            terminated = true;
            StopFrames();
            game.reset();
            // A failed process setup might never have created GameApplication,
            // whose destructor normally releases the platform.
            if (sky::Platform::Get()->GetImpl() != nullptr) {
                sky::Platform::Get()->Shutdown();
            }
            platformReady = false;
        }

    private:
        void StopFrames()
        {
            sky::Platform *platform = sky::Platform::Get();
            if (platform->GetImpl() != nullptr) {
                platform->StopFrameLoop();
                platform->SetFrameCallback({});
            }
        }

        int                                   argumentCount     = 0;
        char                                **arguments         = nullptr;
        bool                                  platformAttempted = false;
        bool                                  platformReady     = false;
        bool                                  gameAttempted     = false;
        bool                                  terminated        = false;
        std::unique_ptr<sky::GameApplication> game;
    };

    IOSGameHost gameHost;

    UIWindowScene *MainWindowScene(UIScene *scene)
    {
        if (![scene isKindOfClass:[UIWindowScene class]] || ![scene.session.role isEqualToString:UIWindowSceneSessionRoleApplication]) {
            return nil;
        }
        return (UIWindowScene *)scene;
    }

} // namespace

@interface SkyLauncherDelegate : UIResponder <UIApplicationDelegate>
@end

@interface SkyLauncherSceneDelegate : UIResponder <UIWindowSceneDelegate>
@end

@implementation SkyLauncherDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    (void)launchOptions;
    // A window scene is not connected yet. Successful process setup is enough
    // to finish launching; the SceneDelegate starts the game after binding it.
    return gameHost.InitPlatform(application) ? YES : NO;
}

- (void)applicationWillTerminate:(UIApplication *)application
{
    (void)application;
    gameHost.Shutdown();
}

@end

@implementation SkyLauncherSceneDelegate

- (void)scene:(UIScene *)scene willConnectToSession:(UISceneSession *)session options:(UISceneConnectionOptions *)connectionOptions
{
    (void)session;
    (void)connectionOptions;
    UIWindowScene *windowScene = MainWindowScene(scene);
    if (windowScene != nil) {
        gameHost.Connect(windowScene);
    }
}

- (void)sceneDidBecomeActive:(UIScene *)scene
{
    UIWindowScene *windowScene = MainWindowScene(scene);
    if (windowScene != nil) {
        gameHost.SetSceneActive(windowScene, true);
    }
}

- (void)sceneWillResignActive:(UIScene *)scene
{
    UIWindowScene *windowScene = MainWindowScene(scene);
    if (windowScene != nil) {
        gameHost.SetSceneActive(windowScene, false);
    }
}

- (void)sceneWillEnterForeground:(UIScene *)scene
{
    UIWindowScene *windowScene = MainWindowScene(scene);
    if (windowScene != nil) {
        gameHost.SetSceneActive(windowScene, false);
    }
}

- (void)sceneDidEnterBackground:(UIScene *)scene
{
    UIWindowScene *windowScene = MainWindowScene(scene);
    if (windowScene != nil) {
        gameHost.SetSceneActive(windowScene, false);
    }
}

- (void)sceneDidDisconnect:(UIScene *)scene
{
    UIWindowScene *windowScene = MainWindowScene(scene);
    if (windowScene != nil) {
        gameHost.Disconnect(windowScene);
    }
}

@end

int main(int argc, char **argv)
{
    gameHost.SetArguments(argc, argv);
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([SkyLauncherDelegate class]));
    }
}

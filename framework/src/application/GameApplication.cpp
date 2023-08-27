//
// Created by Zach Lee on 2022/9/25.
//

#include <framework/application/GameApplication.h>
#include <framework/platform/PlatformBase.h>

namespace sky {

    bool GameApplication::Init(StartInfo &start)
    {
        // platform
        sky::Platform* platform = sky::Platform::Get();
        if (!platform->Init({})) {
            return false;
        }

        nativeWindow.reset(NativeWindow::Create(NativeWindow::Descriptor{start.windowWidth, start.windowHeight, start.appName, start.appName, start.mainWindow}));

        if (!Application::Init(start)) {
            return false;
        }
        return !!nativeWindow;
    }

    void GameApplication::Shutdown()
    {
        Application::Shutdown();
        nativeWindow.reset();
    }

    void GameApplication::PreTick()
    {
        if (nativeWindow) {
            nativeWindow->PollEvent(exit);
        }
    }

    const NativeWindow *GameApplication::GetViewport() const
    {
        return nativeWindow.get();
    }
}
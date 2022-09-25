//
// Created by Zach Lee on 2022/9/25.
//

#include <framework/application/GameApplication.h>

namespace sky {

    bool GameApplication::Init(StartInfo &start)
    {
        nativeWindow.reset(NativeWindow::Create(NativeWindow::Descriptor{start.windowWidth, start.windowHeight, start.appName, start.appName}));

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
        nativeWindow->PollEvent(exit);
    }

    const NativeWindow *GameApplication::GetViewport() const
    {
        return nativeWindow.get();
    }
}
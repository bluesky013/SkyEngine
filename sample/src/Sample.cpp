//
// Created by Zach Lee on 2021/11/26.
//

#include <Sample.h>

namespace sky {

    void Sample::Start()
    {
        NativeWindow::Descriptor des = {
            "SkyEngine",
            "Sample"
        };
        nativeWindow = application.CreateNativeWindow(des);

        viewport = new Viewport(nativeWindow->GetNativeHandle());

        world = new World();

        engine.AddViewport(*viewport);
        engine.AddWorld(*world);

        world->SetTarget(*viewport);
    }

    void Sample::Stop()
    {
        engine.RemoveWorld(*world);
        delete world;
        world = nullptr;

        engine.RemoveViewport(*viewport);
        delete viewport;
        viewport = nullptr;
    }

    void Sample::Tick(float delta)
    {

    }

}
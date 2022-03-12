//
// Created by Zach Lee on 2021/11/26.
//

#include <Sample.h>
#include <framework/asset/AssetManager.h>

namespace sky {

    void Sample::Start()
    {
        NativeWindow::Descriptor des = {
            1366,
            768,
            "SkyEngine",
            "Sample"
        };
        nativeWindow.reset(NativeWindow::Create(des));
    }

    void Sample::Stop()
    {
        nativeWindow.reset();
    }

    void Sample::Tick(float delta)
    {

    }

}
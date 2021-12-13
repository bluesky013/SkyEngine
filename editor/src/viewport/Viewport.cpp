//
// Created by Zach Lee on 2021/12/13.
//

#include <editor/viewport/Viewport.h>
#include <engine/SkyEngine.h>

namespace sky::editor {

    Viewport::Viewport()
    {
    }

    void Viewport::Init()
    {
        setSurfaceType(SurfaceType::MetalSurface);
        viewport = new sky::Viewport((void*)winId());
        SkyEngine::Get()->AddViewport(*viewport);
    }

    void Viewport::Shutdown()
    {
        if (viewport != nullptr) {
            SkyEngine::Get()->RemoveViewport(*viewport);
            delete viewport;
            viewport = nullptr;
        }
    }

}
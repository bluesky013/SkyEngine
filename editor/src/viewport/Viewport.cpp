//
// Created by Zach Lee on 2021/12/13.
//

#include <editor/viewport/Viewport.h>
#include <engine/SkyEngine.h>

namespace sky::editor {

    Viewport::Viewport()
    {
        setSurfaceType(SurfaceType::MetalSurface);
    }

    void Viewport::Init()
    {
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

    sky::Viewport* Viewport::GetNativeViewport() const
    {
        return viewport;
    }

    bool Viewport::event(QEvent *event)
    {
        auto handler = SkyEngine::Get();
        switch (event->type()) {
            case QEvent::Resize:
                handler->OnWindowResize((void*)winId(), geometry().width(), geometry().height());
                break;
            case QEvent::MouseMove:
                break;
            default:
                break;
        }
        return true;
    }

}
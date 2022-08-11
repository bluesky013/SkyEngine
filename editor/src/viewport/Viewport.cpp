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

    Viewport::~Viewport()
    {
        if (viewport != nullptr) {
            delete viewport;
            viewport = nullptr;
        }
    }

    void Viewport::Init()
    {
        viewport = new sky::Viewport((void*)winId());
    }

    sky::Viewport* Viewport::GetNativeViewport() const
    {
        return viewport;
    }

    bool Viewport::event(QEvent *event)
    {
        switch (event->type()) {
            case QEvent::Resize:
                break;
            case QEvent::MouseMove:
                break;
            default:
                break;
        }
        return true;
    }

}
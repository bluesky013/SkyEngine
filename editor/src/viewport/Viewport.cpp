//
// Created by Zach Lee on 2021/12/13.
//

#include <editor/viewport/Viewport.h>
#include <render/Renderer.h>

namespace sky::editor {

    Viewport::Viewport()
    {
        setSurfaceType(SurfaceType::MetalSurface);
    }

    Viewport::~Viewport()
    {
        if (window != nullptr) {
            Renderer::Get()->DestroyRenderWindow(window);
        }
    }

    void Viewport::Init()
    {
        window = Renderer::Get()->CreateRenderWindow(reinterpret_cast<void *>(winId()), width(), height(), true);
    }

    bool Viewport::event(QEvent *event)
    {
        switch (event->type()) {
        case QEvent::Resize:
            if (window != nullptr) {
                window->Resize(reinterpret_cast<void *>(winId()), width(), height());
            }
            break;
        case QEvent::MouseMove: break;
        default: break;
        }
        return true;
    }

} // namespace sky::editor

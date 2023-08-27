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
            ViewportManager::Get()->UnRegister(viewportId);
        }
    }

    void Viewport::Init(ViewportID id)
    {
        window = Renderer::Get()->CreateRenderWindow(reinterpret_cast<void *>(winId()), width(), height(), true);
        viewportId = id;
        ViewportManager::Get()->Register(viewportId, this);
    }

    bool Viewport::event(QEvent *event)
    {
        switch (event->type()) {
        case QEvent::Resize:
            if (window != nullptr) {
                window->Resize(reinterpret_cast<void *>(winId()), width(), height());
            }
            break;
        case QEvent::MouseMove:
            break;
        case QEvent::Close:
            break;
        default: break;
        }
        return true;
    }

    void ViewportManager::Register(ViewportID key, Viewport* vp)
    {
        viewports.emplace(key, vp);
    }

    void ViewportManager::UnRegister(ViewportID key)
    {
        viewports.erase(key);
    }

    Viewport *ViewportManager::FindViewport(ViewportID key) const
    {
        return viewports.at(key);
    }
} // namespace sky::editor

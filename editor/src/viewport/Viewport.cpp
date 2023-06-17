//
// Created by Zach Lee on 2021/12/13.
//

#include <editor/viewport/Viewport.h>

namespace sky::editor {

    Viewport::Viewport()
    {
        setSurfaceType(SurfaceType::MetalSurface);
    }

    Viewport::~Viewport()
    {
    }

    void Viewport::Init()
    {
    }

    bool Viewport::event(QEvent *event)
    {
        switch (event->type()) {
        case QEvent::Resize: break;
        case QEvent::MouseMove: break;
        default: break;
        }
        return true;
    }

} // namespace sky::editor

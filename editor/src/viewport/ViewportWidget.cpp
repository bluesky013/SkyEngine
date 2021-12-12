//
// Created by Zach Lee on 2021/12/12.
//

#include <editor/viewport/ViewportWidget.h>
#include <engine/SkyEngine.h>

namespace sky::editor {

    ViewportWidget::ViewportWidget(QWidget* parent)
        : QWidget(parent)
        , viewport(nullptr)
    {
    }

    ViewportWidget::~ViewportWidget()
    {
    }

    void ViewportWidget::Init()
    {
        viewport = new Viewport((void*)winId());
        SkyEngine::Get()->AddViewport(*viewport);
    }

    void ViewportWidget::Shutdown()
    {
        if (viewport != nullptr) {
            SkyEngine::Get()->RemoveViewport(*viewport);
            delete viewport;
            viewport = nullptr;
        }
    }

    bool ViewportWidget::event(QEvent *event)
    {
        auto size = rect();
        switch (event->type()) {
            case QEvent::Resize:
                SkyEngine::Get()->OnResize((void*)winId(), size.width(), size.height());
                break;
            default:
                break;
        }
    }

}
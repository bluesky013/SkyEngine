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
        setVisible(true);
        setUpdatesEnabled(false);
        setMouseTracking(true);
        setObjectName("renderOverlay");
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

}
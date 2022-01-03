//
// Created by Zach Lee on 2021/12/12.
//

#include <editor/viewport/ViewportWidget.h>
#include <QGridLayout>
#include <engine/SkyEngine.h>

namespace sky::editor {

    ViewportWidget::ViewportWidget(QWidget* parent)
        : QWidget(parent)
        , window(new Viewport())
    {
    }

    ViewportWidget::~ViewportWidget()
    {
    }

    void ViewportWidget::Init()
    {
        auto container = QWidget::createWindowContainer(window, this);
        window->Init();
        auto layout = new QGridLayout(this);
        layout->setMargin(0);
        layout->addWidget(container, 0, 0, 1, 1);
    }

    void ViewportWidget::Shutdown()
    {
        if (window != nullptr) {
            window->Shutdown();
        }
    }

    sky::Viewport* ViewportWidget::GetNativeViewport() const
    {
        return window->GetNativeViewport();
    }

    bool ViewportWidget::event(QEvent *event)
    {
        switch (event->type()) {
            case QEvent::Resize:
                window->setGeometry(geometry());
                SkyEngine::Get()->OnResize((void*)window->winId(), geometry().width(), geometry().height());
                break;
            default:
                break;
        }
        return true;
    }

}
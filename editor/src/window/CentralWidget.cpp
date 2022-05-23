//
// Created by Zach Lee on 2021/12/12.
//

#include "CentralWidget.h"
#include <QVBoxLayout>

namespace sky::editor {

    CentralWidget::CentralWidget(QWidget* parent)
        : QWidget(parent)
        , viewport(new ViewportWidget(this))
    {
    }

    CentralWidget::~CentralWidget()
    {

    }

    void CentralWidget::Init()
    {
        viewport->Init();
        auto layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(viewport);
    }

    void CentralWidget::Shutdown()
    {
        if (viewport != nullptr) {
            viewport->Shutdown();
            delete viewport;
            viewport = nullptr;
        }
    }

    ViewportWidget* CentralWidget::GetViewport() const
    {
        return viewport;
    }

    bool CentralWidget::event(QEvent *event)
    {
        auto rect = geometry();
        switch (event->type()) {
            case QEvent::Resize:
                break;
            default:
                break;
        }
        return true;
    }

}
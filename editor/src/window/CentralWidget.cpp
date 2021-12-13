//
// Created by Zach Lee on 2021/12/12.
//

#include "CentralWidget.h"
#include <QGridLayout>

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
        auto layout = new QGridLayout(this);
        layout->setMargin(0);
        layout->addWidget(viewport, 0, 0, 1, 1);
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
        auto size = rect();
        switch (event->type()) {
            case QEvent::Resize:
                break;
            default:
                break;
        }
        return true;
    }

}
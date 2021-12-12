//
// Created by Zach Lee on 2021/12/12.
//

#include "CentralWidget.h"
#include <QBoxLayout>

namespace sky::editor {

    CentralWidget::CentralWidget(QWidget* parent)
        : QWidget(parent)
    {
        Init();
    }

    CentralWidget::~CentralWidget()
    {

    }

    void CentralWidget::Init()
    {
        auto layout = new QHBoxLayout();
        viewport = new ViewportWidget();
        layout->addWidget(viewport);
        viewport->Init();
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

}
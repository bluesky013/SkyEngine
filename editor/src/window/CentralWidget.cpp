//
// Created by Zach Lee on 2021/12/12.
//

#include "CentralWidget.h"

namespace sky::editor {

    CentralWidget::CentralWidget(QWidget* parent)
        : QMainWindow(parent)
    {
        Init();
    }

    CentralWidget::~CentralWidget()
    {

    }

    void CentralWidget::Init()
    {
        viewport = new ViewportWidget(this);
    }

    void CentralWidget::Shutdown()
    {
        viewport->Shutdown();
    }

    ViewportWidget* CentralWidget::GetViewport() const
    {
        return viewport;
    }

}
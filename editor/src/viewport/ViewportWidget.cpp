//
// Created by Zach Lee on 2021/12/12.
//

#include <QPushButton>
#include <QVBoxLayout>
#include <editor/viewport/ViewportWidget.h>
#include <engine/SkyEngine.h>

namespace sky::editor {

    ViewportWidget::ViewportWidget(QWidget *parent) : QWidget(parent), window(new Viewport())
    {
    }

    ViewportWidget::~ViewportWidget()
    {
    }

    void ViewportWidget::Init()
    {
        auto container = QWidget::createWindowContainer(window, this, Qt::Widget);
        window->Init();
        auto layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(container);
    }

} // namespace sky::editor

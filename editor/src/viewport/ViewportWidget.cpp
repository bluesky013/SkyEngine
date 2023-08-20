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

    void ViewportWidget::Init()
    {
        auto *container = QWidget::createWindowContainer(window, this, Qt::Widget);
        window->Init(ViewportID::EDITOR_PREVIEW);
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(container);
    }

} // namespace sky::editor

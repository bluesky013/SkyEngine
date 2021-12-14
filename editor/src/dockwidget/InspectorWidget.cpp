//
// Created by Zach Lee on 2021/12/15.
//

#include <editor/dockwidget/InspectorWidget.h>
#include <QPushButton>
#include <QVBoxLayout>

namespace sky::editor {

    InspectorWidget::InspectorWidget(QWidget* parent)
        : QDockWidget(parent)
    {
        setWindowTitle(tr("Inspector"));
        auto widget = new QWidget(this);
        setWidget(widget);
        auto layout = new QVBoxLayout(widget);
        auto button = new QPushButton(tr("Add Component"), widget);
        layout->addWidget(button);
    }

}
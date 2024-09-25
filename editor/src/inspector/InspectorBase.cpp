//
// Created by Zach Lee on 2021/12/15.
//

#include <QApplication>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>
#include <editor/inspector/InspectorBase.h>
#include <editor/framework/ReflectedObjectWidget.h>

namespace sky::editor {

    InspectorBase::InspectorBase(QWidget *parent) : QWidget(parent)
    {
        layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        label = new QLabel(this);
        label->setStyleSheet("background-color: #999999; border-radius: 8px; border-style: solid; border-color: #1B1B1B; border-width: 1px; "
                             "border-left: none; border-right: none;"
                             "font-size: 12pt;");
        label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        label->setFixedHeight(24);
        label->setMargin(0);

        layout->addWidget(label);
    }

    void InspectorBase::SetObject(void *ptr, const TypeNode *node)
    {
        label->setText(node->info->name.data());
        auto *prop = new ReflectedObjectWidget(ptr, node, this);
        layout->addWidget(prop);
    }

} // namespace sky::editor
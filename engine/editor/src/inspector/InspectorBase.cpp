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
        label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
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

//
// Created by Zach Lee on 2021/12/15.
//

#include <QApplication>
#include <QLabel>
#include <QPainter>
#include <QStyle>
#include <QVBoxLayout>
#include <editor/inspector/InspectorBase.h>
#include <editor/inspector/PropertyUtil.h>
#include <editor/inspector/PropertyWidget.h>
#include <editor/inspector/PropertyVectorX.h>

namespace sky::editor {

    InspectorBase::InspectorBase(QWidget *parent) : QWidget(parent)
    {
        layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        label = new QLabel(this);
        label->setStyleSheet("background-color: #333333; border-radius: 8px; border-style: solid; border-color: #1B1B1B; border-width: 1px; "
                             "border-left: none; border-right: none;");
        label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        label->setFixedHeight(24);
        label->setMargin(0);

        layout->addWidget(label);
    }

    void InspectorBase::SetObject(void *ptr, const TypeNode *node)
    {
        label->setText(node->info->markedName.data());
        auto *prop = new PropertyWidget(label);
        prop->SetInstance(ptr, node);
        layout->addWidget(prop);
    }

} // namespace sky::editor
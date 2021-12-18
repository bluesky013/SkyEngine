//
// Created by Zach Lee on 2021/12/16.
//

#include <editor/inspector/PropertyWidget.h>
#include <framework/serialization/SerializationContext.h>
#include <QLabel>

namespace sky::editor {

    PropertyWidget::PropertyWidget(QWidget* parent)
        : QWidget(parent)
        , instance(nullptr)
        , memberNode(nullptr)
    {
        label = new QLabel(this);
        label->setFixedWidth(80);
    }

    void PropertyWidget::SetInstance(void* inst, const QString& name, const TypeMemberNode& node)
    {
        instance = inst;
        memberNode = &node;
        label->setText(name);
        Refresh();
    }
}
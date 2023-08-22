//
// Created by Zach Lee on 2021/12/16.
//

#include <QLabel>
#include <QHBoxLayout>
#include <editor/inspector/PropertyWidget.h>
#include <editor/inspector/PropertyUtil.h>
#include <editor/inspector/PropertyVectorX.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::editor {

    PropertyWidget::PropertyWidget(void *ptr, const TypeNode *node, QWidget* parent) : QWidget(parent), instance(ptr), typeNode(node)
    {
        label = new QLabel(this);

        layout = new QHBoxLayout(this);
        layout->addWidget(label);

        setLayout(layout);
        setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        layout->setContentsMargins(0, 0, 0, 0);
    }

    PropertyWidget::PropertyWidget(QWidget *parent) : PropertyWidget(nullptr, nullptr, parent)
    {
    }

    void PropertyWidget::SetLabel(const QString &str)
    {
        label->setFixedWidth(120);
        label->setText(str);
    }

    void PropertyWidget::SetInstance(void *inst, const TypeNode *node)
    {
        instance = inst;
        typeNode = node;

        if (node->info->typeId == TypeInfoObj<bool>::Get()->RtInfo()->typeId) {
            auto *widget = new PropertyBool(inst, node, this);
            layout->addWidget(widget);
            children.push_back(widget);
        } else if (node->info->typeId == TypeInfoObj<float>::Get()->RtInfo()->typeId) {
            auto *widget = new PropertyScalar<float>(inst, node, this);
            layout->addWidget(widget);
            children.push_back(widget);
        } else if (node->info->typeId == TypeInfoObj<double>::Get()->RtInfo()->typeId) {
            auto *widget = new PropertyScalar<double>(inst, node, this);
            layout->addWidget(widget);
            children.push_back(widget);
        } else if (node->info->typeId == TypeInfoObj<int32_t>::Get()->RtInfo()->typeId) {
            auto *widget = new PropertyScalar<int32_t>(inst, node, this);
            layout->addWidget(widget);
            children.push_back(widget);
        } else if (node->info->typeId == TypeInfoObj<uint32_t>::Get()->RtInfo()->typeId) {
            auto *widget = new PropertyScalar<uint32_t>(inst, node, this);
            layout->addWidget(widget);
            children.push_back(widget);
        } else {
            auto *childWidget = new QWidget(this);
            auto *childLayout = new QVBoxLayout(childWidget);
            childWidget->setLayout(childLayout);

            childLayout->setSpacing(0);
            childLayout->setContentsMargins(0, 0, 0, 0);
            childWidget->setContentsMargins(0, 0, 0, 0);

            for (const auto &member : node->members) {
                if (!util::CheckProperty(member.second.properties, UI_PROP_VISIBLE)) {
                    continue;
                }
                const auto *childNode = GetTypeNode(member.second.info);
                if (childNode == nullptr) {
                    continue;
                }
                auto *ptr = member.second.getterFn(instance);
                auto *widget = new PropertyWidget(this);

                if (util::CheckProperty(member.second.properties, UI_LABEL_VISIBLE)) {
                    widget->SetLabel(member.first.data());
                }

                widget->SetInstance(ptr, childNode);
                childLayout->addWidget(widget, 0, Qt::AlignLeft | Qt::AlignTop);
                children.push_back(widget);
            }

            layout->addWidget(childWidget);
        }

        Refresh();
    }

    void PropertyWidget::Refresh()
    {
        for (auto &child : children) {
            child->Refresh();
        }
    }
} // namespace sky::editor
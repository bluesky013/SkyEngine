//
// Created by Zach Lee on 2021/12/15.
//

#include <QPushButton>
#include <QVBoxLayout>
#include <QMenu>
#include <editor/inspector/InspectorBase.h>
#include <editor/inspector/InspectorWidget.h>
#include <editor/inspector/PropertyUtil.h>

#include <framework/world/Component.h>
#include <framework/world/GameObject.h>

namespace sky::editor {

    InspectorWidget::InspectorWidget(QWidget *parent) : QDockWidget(parent)
    {
        setWindowTitle(tr("Inspector"));

        auto widget = new QWidget(this);
        setWidget(widget);
        auto rootLayout = new QVBoxLayout(widget);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setAlignment(Qt::AlignTop);

        button = new QPushButton(tr("Add Component"), widget);
        connect(button, &QPushButton::clicked, this, [this](bool checked) {
            QMenu menu(tr("Components"), this);

            auto &types = ComponentFactory::Get()->GetComponentTypes();
            auto *serializationContext = SerializationContext::Get();
            for (auto &type : types) {
                auto *node = serializationContext->FindTypeById(type.first);
                if (node == nullptr) {
                    continue;
                }

                auto action = new QAction(node->info->markedName.data(), &menu);
                connect(action, &QAction::triggered, this, [&](bool checked) {
                    selectedItem->go->AddComponent(type.first);
                    Refresh();
                });

                menu.addAction(action);
            }

            menu.exec(mapToGlobal(button->pos()));
        });

        groupWidget = new QWidget(widget);

        rootLayout->addWidget(groupWidget);
        rootLayout->addWidget(button);

        layout = new QVBoxLayout(groupWidget);
        layout->setAlignment(Qt::AlignTop);
        layout->setContentsMargins(0, 0, 0, 0);
    }

    void InspectorWidget::AddComponent(Component *comp)
    {
        const TypeInfoRT *info = comp->GetTypeInfo();
        auto              node = GetTypeNode(info);
        if (node == nullptr) {
            return;
        }
        auto inspector = new InspectorBase(groupWidget);
        inspector->SetName(node->info->markedName.data());

        layout->addWidget(inspector);
        for (auto &mem : node->members) {
            if (!util::IsVisible(mem.second)) {
                continue;
            }
            auto prop = util::CreateByTypeMemberInfo(mem.second, groupWidget);
            prop->SetInstance(comp, mem.first.data(), mem.second);
            layout->addWidget(prop);
        }

        groups.emplace_back(inspector);
    }

    void InspectorWidget::Clear()
    {
        while (QLayoutItem *child = layout->takeAt(0)) {
            delete child->widget();
            delete child;
        }
        groups.clear();
    }

    void InspectorWidget::SetWorldItem(WorldItem *item)
    {
        selectedItem = item;
        Refresh();
    }

    void InspectorWidget::Refresh()
    {
        if (selectedItem == nullptr) {
            return;
        }
        Clear();
        auto  go    = selectedItem->go;
        auto &comps = go->GetComponents();
        for (auto &comp : comps) {
            AddComponent(comp);
        }
    }

} // namespace sky::editor

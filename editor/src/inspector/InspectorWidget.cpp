//
// Created by Zach Lee on 2021/12/15.
//

#include <editor/inspector/InspectorWidget.h>
#include <editor/inspector/InspectorBase.h>
#include <QPushButton>
#include <QVBoxLayout>
#include <engine/world/GameObject.h>

namespace sky::editor {

    InspectorWidget::InspectorWidget(QWidget* parent)
        : QDockWidget(parent)
    {
        setWindowTitle(tr("Inspector"));
        auto widget = new QWidget(this);
        setWidget(widget);
        auto rootLayout = new QVBoxLayout(widget);
        auto button = new QPushButton(tr("Add Component"), widget);
        groupWidget = new QWidget(widget);

        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(0);
        rootLayout->addWidget(groupWidget);
        rootLayout->addWidget(button);

        layout = new QVBoxLayout(groupWidget);
        layout->setSpacing(0);
        layout->setMargin(0);
    }

    void InspectorWidget::AddComponent()
    {
        auto inspector = new InspectorBase(groupWidget);
        layout->addWidget(inspector);
        groups.emplace_back(inspector);
    }

    void InspectorWidget::Clear()
    {
        while (QLayoutItem* child = layout->takeAt(0))
        {
            delete child->widget();
            delete child;
        }
        groups.clear();
    }

    void InspectorWidget::SetWorldItem(WorldItem* item)
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
        auto go = selectedItem->go;
        auto& comps = go->GetComponents();
        for (auto& comp : comps) {
            AddComponent();
        }
    }

}
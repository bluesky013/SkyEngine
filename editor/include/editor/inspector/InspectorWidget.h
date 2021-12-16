//
// Created by Zach Lee on 2021/12/15.
//

#pragma once

#include <QDockWidget>
#include <vector>
#include <editor/dockwidget/WorldWidget.h>
#include <editor/inspector/InspectorBase.h>

class QVBoxLayout;

namespace sky {
    class World;
    class GameObject;
    class Component;
}

namespace sky::editor {

    class InspectorWidget : public QDockWidget {
    public:
        InspectorWidget(QWidget* parent);
        ~InspectorWidget() = default;

        void SetWorldItem(WorldItem* item);

        void AddComponent(Component* comp);

        void Clear();

    private:
        void Refresh();

        WorldItem* selectedItem;
        QVBoxLayout* layout;
        QWidget* groupWidget;
        std::vector<InspectorBase*> groups;
    };

}
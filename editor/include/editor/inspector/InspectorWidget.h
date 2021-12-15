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
}

namespace sky::editor {

    class InspectorWidget : public QDockWidget {
    public:
        InspectorWidget(QWidget* parent);
        ~InspectorWidget() = default;

        void SetWorldItem(WorldItem* item);

        void AddComponent();

        void Clear();

    private:
        void Refresh();

        WorldItem* selectedItem;
        QVBoxLayout* layout;
        QWidget* groupWidget;
        std::vector<InspectorBase*> groups;
    };

}
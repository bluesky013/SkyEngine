//
// Created by Zach Lee on 2021/12/14.
//

#pragma once
#include <QDockWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTimer>

namespace sky {
    class World;
    class GameObject;
}

namespace sky::editor {

    class WorldItem : public QTreeWidgetItem {
    public:
        explicit WorldItem(QTreeWidget *treeview, int type = Type) : QTreeWidgetItem(treeview, type) {}
        explicit WorldItem(QTreeWidgetItem *parent, int type = Type) : QTreeWidgetItem(parent, type) {}

        GameObject* go = nullptr;
    };

    class WorldWidget : public QDockWidget {
    public:
        WorldWidget(QWidget* parent = nullptr);
        ~WorldWidget() = default;

        void SetWorld(World& world);

    private:
        void Refresh();

        QTimer* refreshTimer;
        World* world;
        QTreeWidget* worldTree;
        WorldItem* rootItem;
    };

}
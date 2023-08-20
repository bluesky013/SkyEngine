//
// Created by Zach Lee on 2021/12/14.
//

#include <editor/dockwidget/WorldWidget.h>
#include <QVBoxLayout>
#include <QMenu>
#include <editor/dockwidget/DockManager.h>
#include <editor/inspector/InspectorWidget.h>

#include <framework/world/GameObject.h>
#include <framework/world/World.h>
#include <framework/world/TransformComponent.h>

namespace sky::editor {

    static void RefreshTree(GameObject& object, WorldItem* item)
    {
        auto *trans = object.GetComponent<TransformComponent>();
        item->setText(0, object.GetName().c_str());
        const auto& children = trans->GetChildren();
        for (const auto& child : children) {
            auto *childItem = new WorldItem(item);
            childItem->go = child->object;
            RefreshTree(*child->object, childItem);
        }
    }

    WorldWidget::WorldWidget(QWidget* parent) : QDockWidget(parent)
    {
        setWindowTitle("Hierarchy");
        worldTree = new QTreeWidget(this);
        worldTree->setHeaderLabel("World");
        setWidget(worldTree);

        setContextMenuPolicy(Qt::CustomContextMenu);
        connect(this, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
            QMenu menu(tr("World Action"), this);
            auto addAct = new QAction(tr("Add"), &menu);
            connect(addAct, &QAction::triggered, this, [this]() {
                if (world == nullptr) {
                    return;
                }

                auto go = world->CreateGameObject("GameObject");
                auto items = worldTree->selectedItems();
                auto parent = static_cast<WorldItem*>(items.empty() ? rootItem : items[0]);
                auto item = new WorldItem(parent);
                item->go = go;
                go->SetParent(parent->go);
                RefreshTree(*go, item);
            });

            menu.addAction(addAct);

            menu.exec(mapToGlobal(pos));
        });

        connect(worldTree, &QTreeWidget::itemClicked, this, [](QTreeWidgetItem* item, int column) {
            WorldItem* worldItem = static_cast<WorldItem*>(item);
            auto inspector = static_cast<InspectorWidget*>(DockManager::Get()->Find((uint32_t)DockId::INSPECTOR));
            inspector->SetWorldItem(worldItem);
        });

//        connect(worldTree, &QTreeWidget::itemDoubleClicked, this, [](QTreeWidgetItem* item, int column) {
//
//        });
    }

    void WorldWidget::Refresh()
    {
        worldTree->clear();
        if (world != nullptr) {
            auto root = world->GetRoot();
            rootItem = new WorldItem(worldTree);
            rootItem->go = root;
            RefreshTree(*root, rootItem);
        }
    }

    void WorldWidget::SetWorld(const WorldPtr &w)
    {
        world = w;
        Refresh();

//        refreshTimer = new QTimer(this);
//        connect(refreshTimer, &QTimer::timeout, this, [this]() {
//            Refresh();
//        });
//        refreshTimer->setInterval(1000);
//        refreshTimer->start();
    }

}

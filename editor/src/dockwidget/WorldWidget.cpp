//
// Created by Zach Lee on 2021/12/14.
//

#include <editor/dockwidget/WorldWidget.h>
#include <editor/dockwidget/DockManager.h>
#include <framework/world/Actor.h>

#include <QVBoxLayout>
#include <QMenu>

namespace sky::editor {

    WorldWidget::WorldWidget(QWidget* parent) : QDockWidget(parent)
    {
        setWindowTitle("Outline");
        treeView = new WorldTreeView(this);
        setWidget(treeView);
    }

    void WorldWidget::SetWorld(const WorldPtr &world)
    {
        treeView->BuildByWorld(world);

//        refreshTimer = new QTimer(this);
//        connect(refreshTimer, &QTimer::timeout, this, [this]() {
//            Refresh();
//        });
//        refreshTimer->setInterval(1000);
//        refreshTimer->start();
    }

}

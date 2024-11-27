//
// Created by Zach Lee on 2021/12/14.
//

#pragma once
#include <QDockWidget>
#include <QTimer>
#include <framework/world/World.h>
#include <editor/framework/WorldTreeView.h>

namespace sky::editor {

    class WorldWidget : public QDockWidget {
    public:
        explicit WorldWidget(QWidget* parent = nullptr);
        ~WorldWidget() override = default;

        void SetWorld(const WorldPtr& world);
        WorldTreeView *GetWorldTreeView() const { return treeView; }
    private:
        QTimer* refreshTimer = nullptr;
        WorldTreeView *treeView = nullptr;
    };

}
//
// Created by Zach Lee on 2023/1/14.
//

#pragma once

#include <QDockWidget>
#include <QFileSystemModel>
#include <QTreeView>

namespace sky::editor {

    class AssetWidget : public QDockWidget {
    public:
        explicit AssetWidget(QWidget *parent);
        ~AssetWidget() override = default;

    private:
        QFileSystemModel *fsModel = nullptr;
        QTreeView *treeView = nullptr;
    };

}

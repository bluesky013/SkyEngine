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
        AssetWidget(QWidget *parent);
        ~AssetWidget() = default;

        void OnProjectChange(const QString &projectPath);

    private:
        QFileSystemModel *fsModel = nullptr;
        QTreeView *treeView = nullptr;
    };

}
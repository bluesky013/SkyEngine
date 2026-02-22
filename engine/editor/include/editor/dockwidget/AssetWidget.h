//
// Created by Zach Lee on 2023/1/14.
//

#pragma once

#include <QDockWidget>
#include <QFileSystemModel>
#include <QTreeView>
#include <QFrame>
#include <QComboBox>

namespace sky::editor {

    class AssetDirBrowser : public QFrame {
    public:
        explicit AssetDirBrowser(QWidget *parent);
        ~AssetDirBrowser() override = default;

    private:
        void OnContentMenuClicked(const QPoint &pos);

        std::unordered_map<uint32_t, QFileSystemModel*> models;
        std::unordered_map<uint32_t, QModelIndex> modelRoot;
        QTreeView *treeView = nullptr;

        uint32_t currentModel = 0;
    };

    class AssetWidget : public QDockWidget {
    public:
        explicit AssetWidget(QWidget *parent);
        ~AssetWidget() override = default;
    };

}

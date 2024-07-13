//
// Created by blues on 2024/7/12.
//

#pragma once

#include <QWidget>
#include <QStandardItemModel>
#include <QTreeView>
#include <QListView>
#include <QLineEdit>
#include <QComboBox>
#include <QSortFilterProxyModel>
#include <editor/framework/AssetEditorProxy.h>

namespace sky::editor {

    class AssetFilterProxyModel : public QSortFilterProxyModel {
    public:
        explicit AssetFilterProxyModel(QObject *parent) : QSortFilterProxyModel(parent) {}
        ~AssetFilterProxyModel() override = default;

    private:
        bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    };

    class AssetBrowserWidget : public QWidget {
    public:
        explicit AssetBrowserWidget(QWidget *parent);
        ~AssetBrowserWidget() override = default;

    private:
        QListView* assetItemView = nullptr;
        QLineEdit* filter = nullptr;
        QComboBox* categoryBox = nullptr;
    };


} // namespace sky::editor

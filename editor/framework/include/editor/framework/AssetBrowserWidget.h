//
// Created by blues on 2024/7/12.
//

#pragma once

#include <QWidget>
#include <QStandardItemModel>
#include <QTreeView>
#include <QLineEdit>
#include <QListView>
#include <QTableView>
#include <QSortFilterProxyModel>
#include <QMouseEvent>
#include <editor/framework/AssetEditorProxy.h>

namespace sky::editor {

    class AssetFilterProxyModel : public QSortFilterProxyModel {
    public:
        explicit AssetFilterProxyModel(QObject *parent) : QSortFilterProxyModel(parent) {}
        ~AssetFilterProxyModel() override = default;

    private:
        bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    };

    class AssetListView : public QListView {
    public:
        explicit AssetListView(QWidget *parent) : QListView(parent) {}
        ~AssetListView() override = default;

        void SetAssetBundle(SourceAssetBundle bundle) { assetBundle = bundle; }
        SourceAssetBundle GetAssetBundle() const { return assetBundle; }
    private:
        void mousePressEvent(QMouseEvent *event) override;
        void mouseMoveEvent(QMouseEvent *event) override;

        QPoint startPos = {};
        SourceAssetBundle assetBundle;
    };

    class AssetBrowserWidget : public QWidget {
    public:
        explicit AssetBrowserWidget(QWidget *parent);
        ~AssetBrowserWidget() override = default;

    private:
        void ResetView();
        void OnContentMenuClicked(const QPoint &pos);

        AssetListView* assetItemView = nullptr;
        QLineEdit* filter = nullptr;
        QComboBox* comboBox = nullptr;
    };


} // namespace sky::editor

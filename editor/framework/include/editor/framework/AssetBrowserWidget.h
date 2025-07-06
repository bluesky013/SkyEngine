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
#include <QDialog>
#include <editor/framework/AssetEditorProxy.h>
#include <core/environment/Singleton.h>

#include <unordered_map>

class QComboBox;

namespace sky::editor {

    class AssetPreviewContentWidget : public QWidget {
    public:
        AssetPreviewContentWidget() = default;
        ~AssetPreviewContentWidget() = default;

        virtual void OnClose() = 0;
    };

    class AssetPreviewWidget : public QWidget {
    public:
        AssetPreviewWidget();
        ~AssetPreviewWidget();

        void SetWidget(AssetPreviewContentWidget* widget);

    private:
        bool event(QEvent *event) override;

        AssetPreviewContentWidget* content = nullptr;
    };

    class IAssetPreviewWndFactory {
    public:
        IAssetPreviewWndFactory() = default;
        virtual ~IAssetPreviewWndFactory() = default;

        virtual bool SetupWidget(AssetPreviewWidget& widget, const AssetSourcePtr& src) = 0;
    };

    class AssetPreviewManager : public Singleton<AssetPreviewManager> {
    public:
        AssetPreviewManager() = default;
        ~AssetPreviewManager() = default;

        void Register(const std::string_view &type, IAssetPreviewWndFactory* factory)
        {
            factories[type].reset(factory);
        }

        IAssetPreviewWndFactory* Find(const std::string_view& type)
        {
            auto iter = factories.find(type);
            return iter != factories.end() ? iter->second.get() : nullptr;
        }

    private:
        std::unordered_map<std::string_view, std::unique_ptr<IAssetPreviewWndFactory>> factories;
    };

    struct ImportSettingDlg : public QDialog {
    public:
        explicit ImportSettingDlg(const QString& src);
        ~ImportSettingDlg() = default;

        const AssetImportRequest& GetConfig() const { return config; }
    private:
        AssetImportRequest config;
    };

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

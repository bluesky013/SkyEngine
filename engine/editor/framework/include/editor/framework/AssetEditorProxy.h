//
// Created by blues on 2024/7/12.
//

#pragma once

#include <framework/asset/AssetDataBase.h>
#include <core/environment/Singleton.h>
#include <core/event/Event.h>
#include <QStandardItemModel>
#include <QFileSystemModel>
#include <QStandardItem>

namespace sky::editor {

    class AssetItem : public QStandardItem {
    public:
        explicit AssetItem(const AssetSourcePtr &ptr);
        ~AssetItem() override = default;

    private:
        AssetSourcePtr source;
    };

    class AssetItemModel : public QStandardItemModel {
    public:
        explicit AssetItemModel(QObject *parent) : QStandardItemModel(parent) {}
        ~AssetItemModel() override = default;
    };

    class AssetDataBaseProxy : public Singleton<AssetDataBaseProxy> {
    public:
        AssetDataBaseProxy();
        ~AssetDataBaseProxy() override = default;

        AssetItemModel *GetModel() const { return model.get(); }
        QFileSystemModel *GetProjectModel() const { return projectModel.get(); }
        QFileSystemModel *GetEngineModel() const { return engineModel.get(); }

        const std::unordered_map<std::string, QModelIndex>& GetCategoryTable() const { return categoryLut; }

    private:
        void Refresh();

        std::unique_ptr<AssetItemModel> model;
        std::unique_ptr<QFileSystemModel> projectModel;
        std::unique_ptr<QFileSystemModel> engineModel;
        std::unordered_map<std::string, QModelIndex> categoryLut;
    };

} // namespace sky::editor
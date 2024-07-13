//
// Created by blues on 2024/7/12.
//

#pragma once

#include <framework/asset/AssetDataBase.h>
#include <core/environment/Singleton.h>
#include <core/event/Event.h>
#include <QStandardItemModel>
#include <QStandardItem>

namespace sky::editor {

    class AssetItem : public QStandardItem {
    public:
        explicit AssetItem(const AssetSourcePtr &ptr) : QStandardItem(ptr->name.c_str()), source(ptr) {}
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

        AssetItemModel *GetModel() const { return model; }
        const std::unordered_map<std::string, QModelIndex>& GetCategoryTable() const { return categoryLut; }

    private:
        void Refresh();

        AssetItemModel *model = nullptr;
        std::unordered_map<std::string, QModelIndex> categoryLut;
    };

} // namespace sky::editor
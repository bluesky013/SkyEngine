//
// Created by blues on 2024/7/12.
//

#include <editor/framework/AssetEditorProxy.h>

namespace sky::editor {

    AssetItem::AssetItem(const AssetSourcePtr &ptr)
        : QStandardItem(ptr->path.path.FileNameWithoutExt().c_str()), source(ptr)
    {
        setIcon(QIcon::fromTheme("folder"));
        setDragEnabled(true);
    }

    AssetDataBaseProxy::AssetDataBaseProxy()
    {
        model = std::make_unique<AssetItemModel>(nullptr);
        projectModel = std::make_unique<QFileSystemModel>(nullptr);
        projectModel->setRootPath(AssetDataBase::Get()->GetWorkSpaceFs()->GetPath().GetStr().c_str());
        engineModel = std::make_unique<QFileSystemModel>(nullptr);
        engineModel->setRootPath(AssetDataBase::Get()->GetEngineFs()->GetPath().GetStr().c_str());
        Refresh();
    }

    void AssetDataBaseProxy::Refresh()
    {
        auto *db = AssetDataBase::Get();
        const auto &sources = db->GetSources();

        model->invisibleRootItem()->setText("Root");
        for (const auto &[id, source] : sources) {
            auto iter = categoryLut.find(source->category);
            if (iter == categoryLut.end()) {
                auto *item = new QStandardItem(source->category.c_str());
                model->appendRow(item);

                iter = categoryLut.emplace(source->category, item->index()).first;
            }

            auto *item = new AssetItem(source);
            model->itemFromIndex(iter->second)->appendRow(item);
        }
    }

} // namespace sky::editor
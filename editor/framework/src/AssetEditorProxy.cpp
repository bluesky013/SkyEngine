//
// Created by blues on 2024/7/12.
//

#include <editor/framework/AssetEditorProxy.h>

namespace sky::editor {

    AssetDataBaseProxy::AssetDataBaseProxy()
    {
        model = new AssetItemModel(nullptr);
        Refresh();
    }

    void AssetDataBaseProxy::Refresh()
    {
        auto *db = AssetDataBase::Get();
        const auto &sources = db->GetSources();

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
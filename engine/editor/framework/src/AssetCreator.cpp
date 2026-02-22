//
// Created by blues on 2024/12/7.
//

#include <editor/framework/AssetCreator.h>
#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetDataBase.h>

namespace sky::editor {

    void AssetCreatorManager::RegisterTool(const Name &key, AssetCreatorBase* tool)
    {
        tools[key].reset(tool);
    }

    void AssetCreatorManager::UnRegisterTool(const Name &key)
    {
        auto iter = tools.find(key);
        if (iter != tools.end()) {
            tools.erase(iter);
        }
    }

} // namespace sky::editor
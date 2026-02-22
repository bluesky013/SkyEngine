//
// Created by blues on 2025/5/18.
//

#include <editor/framework/EditorActorCreation.h>

namespace sky::editor {

    void EditorActorCreation::RegisterCreation(IActorCreateBase* tool)
    {
        creatFn.emplace_back(tool);
    }

    void EditorActorCreation::UnRegisterCreation(IActorCreateBase* tool)
    {
        auto iter = std::find(creatFn.begin(), creatFn.end(), tool);
        if (iter != creatFn.end())
        {
            creatFn.erase(iter);
        }
    }

} // namespace sky::editor
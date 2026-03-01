//
// Created by blues on 2024/11/28.
//

#include <editor/framework/EditorToolBase.h>

namespace sky::editor {

    void EditorToolManager::RegisterTool(const Name &key, ToolBase* tool)
    {
        if (tool->Init()) {
            tools[key].reset(tool);
        }
    }

    void EditorToolManager::UnRegisterTool(const Name &key)
    {
        auto iter = tools.find(key);
        if (iter != tools.end()) {
            iter->second->Shutdown();
            tools.erase(iter);
        }
    }

} // namespace sky::editor
//
// Created by blues on 2024/11/28.
//

#pragma once

#include <editor/framework/EditorToolBase.h>

namespace sky::editor {

    class SelectTool : public ToolBase {
    public:
        SelectTool() = default;
        ~SelectTool() override = default;

    private:
        bool Init() override;
        void Shutdown() override;
    };

} // namespace sky::editor
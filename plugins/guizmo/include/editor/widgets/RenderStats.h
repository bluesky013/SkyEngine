//
// Created by blues on 2024/3/30.
//

#pragma once

#include <imgui/ImWidget.h>

namespace sky::editor {

    class RenderStats : public ImWidget {
    public:
        RenderStats() : ImWidget("Statistics") {}
        ~RenderStats() override = default;

        void Execute(ImContext &context) override;
    };

} // namespace sky::editor
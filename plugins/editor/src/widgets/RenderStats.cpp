//
// Created by blues on 2024/3/30.
//

#include <editor/widgets/RenderStats.h>
#include <imgui.h>

namespace sky::editor {

    void RenderStats::Execute(ImContext &context)
    {
        if (ImGui::Begin(name.c_str())) {
//            ImGui::Text("%.2f ms/frame (%.1d fps)", (1000.0f / lastFPS), lastFPS);
        }
        ImGui::End();
    }

} // namespace sky::editor
//
// Created by blues on 2024/4/6.
//

#include <editor/widgets/AssetInspector.h>
#include <imgui.h>

namespace sky::editor {

    void AssetInspector::Execute(ImContext &context)
    {
        if (!show) {
            currentType = Uuid();
            currentData = nullptr;
            return;
        }

        if (ImGui::Begin(name.c_str(), &show)) {
            ImGui::Text("Type %s", currentType.ToString().c_str());
        }
        ImGui::End();
    }

    void AssetInspector::OnClicked(const Uuid& type, void *data)
    {
        currentType = type;
        currentData = data;
        show = true;
    }

} // sky::editor
//
// Created by blues on 2023/9/20.
//

#include <imgui/ImGuiFeatureProcessor.h>

namespace sky {

    void ImGuiFeatureProcessor::Tick(float time)
    {
        for (auto &gui : guis) {
            gui->Tick(time);
        }
    }

    void ImGuiFeatureProcessor::Render(rdg::RenderGraph &rdg)
    {
        for (auto &gui : guis) {
            gui->Render(rdg);
        }
    }

    ImGuiInstance *ImGuiFeatureProcessor::CreateGUIInstance()
    {
        auto *gui = new ImGuiInstance();
        gui->AttachScene(scene);
        return guis.emplace_back(gui).get();
    }

    void ImGuiFeatureProcessor::RemoveGUIInstance(ImGuiInstance *mesh)
    {
        guis.remove_if([mesh](const auto &val) {
            return mesh == val.get();
        });
    }

} // namespace
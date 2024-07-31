//
// Created by blues on 2024/7/31.
//

#include <imgui/ImGuiFeatureProcessor.h>

namespace sky {

    void ImGuiFeatureProcessor::Tick(float time)
    {
        for (auto &inst : guiInstances) {
            inst->Tick(time);
        }
    }

    void ImGuiFeatureProcessor::Render(rdg::RenderGraph &rdg)
    {
        for (auto &inst : guiInstances) {
            inst->Render(rdg);
        }
    }

    ImGuiInstance *ImGuiFeatureProcessor::CreateImGuiInstance()
    {
        return guiInstances.emplace_back(std::make_unique<ImGuiInstance>()).get();
    }

    void ImGuiFeatureProcessor::RemoveImGuiInstance(ImGuiInstance *inst)
    {
        guiInstances.remove_if([inst](const auto &val) {
            return inst == val.get();
        });
    }

} // namespace sky
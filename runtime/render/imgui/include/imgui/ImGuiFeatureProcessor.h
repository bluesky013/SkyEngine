//
// Created by blues on 2024/7/31.
//

#pragma once

#include <render/FeatureProcessor.h>
#include <imgui/ImGuiInstance.h>

namespace sky {

    class ImGuiFeatureProcessor : public IFeatureProcessor {
    public:
        explicit ImGuiFeatureProcessor(RenderScene *scene) : IFeatureProcessor(scene) {}
        ~ImGuiFeatureProcessor() override = default;

        void Tick(float time) override;
        void Render(rdg::RenderGraph &rdg) override;

        ImGuiInstance *CreateImGuiInstance();
        void RemoveImGuiInstance(ImGuiInstance *mesh);

    private:
        std::list<std::unique_ptr<ImGuiInstance>> guiInstances;
    };

} // namespace sky
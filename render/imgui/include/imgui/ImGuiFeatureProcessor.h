//
// Created by blues on 2023/9/20.
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

        ImGuiInstance *CreateGUIInstance();
        void RemoveGUIInstance(ImGuiInstance *mesh);

    private:
        std::list<std::unique_ptr<ImGuiInstance>> guis;
    };

} // namespace sky

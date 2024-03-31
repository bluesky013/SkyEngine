//
// Created by blues on 2023/9/20.
//

#pragma once

#include <core/environment/Singleton.h>
#include <render/resource/Technique.h>
#include <render/resource/ResourceGroup.h>
#include <render/RenderPrimitive.h>
#include <imgui/ImGuiInstance.h>
#include <memory>

namespace sky {
    class ImGuiInstance;

    class ImGuiFeature : public Singleton<ImGuiFeature> {
    public:
        ImGuiFeature() = default;
        ~ImGuiFeature() override = default;

        void Init(const RDGfxTechPtr &tech);
        void Tick(float delta);

        RDResourceGroupPtr RequestResourceGroup();

        ImGuiInstance *GetGuiInstance() const { return guiInstance.get(); };

        const TechniqueInstance &GetDefaultTech() const { return instance; }
        const RDResourceLayoutPtr &GetLayout() const { return resLayout; }
        const rhi::VertexInputPtr &GetVertexDesc() const { return vertexDesc; }
    private:
        TechniqueInstance instance;
        RDResourceLayoutPtr resLayout;
        rhi::VertexInputPtr vertexDesc;
        rhi::DescriptorSetPoolPtr pool;

        std::unique_ptr<ImGuiInstance> guiInstance;
    };

} // namespace sky
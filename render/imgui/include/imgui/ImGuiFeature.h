//
// Created by blues on 2023/9/20.
//

#pragma once

#include <core/environment/Singleton.h>
#include <render/resource/Technique.h>
#include <render/resource/ResourceGroup.h>
#include <render/RenderPrimitive.h>

namespace sky {

    class ImGuiFeature : public Singleton<ImGuiFeature> {
    public:
        ImGuiFeature() = default;
        ~ImGuiFeature() override = default;

        void Init(const RDGfxTechPtr &tech);
        RDResourceGroupPtr RequestResourceGroup();

        const TechniqueInstance &GetDefaultTech() const { return instance; }
        const RDResourceLayoutPtr &GetLayout() const { return resLayout; }
        const rhi::VertexInputPtr &GetVertexDesc() const { return vertexDesc; }
    private:
        TechniqueInstance instance;
        RDResourceLayoutPtr resLayout;
        rhi::VertexInputPtr vertexDesc;
        rhi::DescriptorSetPoolPtr pool;
    };

} // namespace sky
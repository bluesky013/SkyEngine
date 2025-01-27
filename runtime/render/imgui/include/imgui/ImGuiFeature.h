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

        void Init();
        void Tick(float delta);

        void SetTechnique(const RDGfxTechPtr &tech) { technique = tech; }
        const RDGfxTechPtr &GetTechnique() const { return technique; }

        const rhi::DescriptorSetPoolPtr &GetPool() const { return pool; }
        const rhi::VertexInputPtr &GetVertexDesc() const { return vertexDesc; }
    private:
        RDGfxTechPtr technique;
        rhi::VertexInputPtr vertexDesc;
        rhi::DescriptorSetPoolPtr pool;
    };

} // namespace sky
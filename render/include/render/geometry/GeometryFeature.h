//
// Created by Zach Lee on 2023/9/3.
//

#pragma once

#include <core/environment/Singleton.h>
#include <render/resource/Technique.h>
#include <render/resource/ResourceGroup.h>

namespace sky {

    class GeometryFeature : public Singleton<GeometryFeature> {
    public:
        GeometryFeature() = default;
        ~GeometryFeature() override = default;

        void Init(const RDGfxTechPtr &tech);
        const RDGfxTechPtr &GetDefaultTech() const { return technique; }
        const rhi::VertexInputPtr &GetVertexDesc() const { return vertexDesc; }
        RDResourceGroupPtr RequestResourceGroup();

    private:
        RDGfxTechPtr technique;
        RDResourceLayoutPtr localLayout;
        rhi::VertexInputPtr vertexDesc;
        rhi::DescriptorSetPoolPtr pool;
    };

} // namespace sky

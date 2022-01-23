//
// Created by Zach Lee on 2021/12/26.
//

#pragma once

#include <engine/render/RenderPipeline.h>
#include <engine/asset/ShaderAsset.h>
#include <vulkan/RenderPass.h>
#include <unordered_map>
#include <set>
#include <list>
#include <string>

namespace sky {
    class RenderGraph;

    class ForwardRendering : public RenderPipeline {
    public:
        ForwardRendering();
        ~ForwardRendering();

        void Render(RenderGraph&) override;

    private:
        void SetupShader();

        void SetupImage();
        std::set<std::string> viewTags;
        drv::ImagePtr colorImage;
        drv::ImagePtr depthImage;
        VkExtent2D extent = {0, 0};
        ShaderPtr shader;
        drv::VertexInputPtr vInput;
        drv::PipelineLayoutPtr pipelineLayout;
    };

}
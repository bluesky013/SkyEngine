//
// Created by Zach Lee on 2021/12/26.
//

#pragma once

#include <render/RenderPipeline.h>
#include <vulkan/RenderPass.h>
#include <vulkan/VertexInput.h>
#include <vulkan/PipelineLayout.h>
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

        void Render(RenderScene& scene, RenderGraph&) override;

    private:
        void SetupShader();

        void SetupImage();
        std::set<std::string> viewTags;
        drv::ImagePtr colorImage;
        drv::ImagePtr depthImage;
        VkExtent2D extent = {0, 0};
        drv::VertexInputPtr vInput;
        drv::PipelineLayoutPtr pipelineLayout;
    };

}
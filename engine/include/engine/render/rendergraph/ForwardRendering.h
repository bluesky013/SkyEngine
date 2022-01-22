//
// Created by Zach Lee on 2021/12/26.
//

#pragma once

#include <engine/render/RenderPipeline.h>
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
        void Setup() override;
        std::set<std::string> viewTags;
        drv::ImagePtr depthImage;
    };

}
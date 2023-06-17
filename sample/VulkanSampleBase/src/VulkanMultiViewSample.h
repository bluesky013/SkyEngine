//
// Created by Zach Lee on 2023/3/30.
//

#pragma once

#include "VulkanSampleBase.h"
#include <core/math/Vector4.h>
#include <future>

namespace sky {
    class NativeWindow;

    class VulkanMultiViewSample : public VulkanSampleBase {
    public:
        VulkanMultiViewSample()  = default;
        ~VulkanMultiViewSample() = default;

        void OnTick(float delta) override;

        void OnStart() override;
        void OnStop() override;
    private:
        void InitFeature() override;

        vk::GraphicsPipelinePtr pso;
        vk::PipelineLayoutPtr   pipelineLayout;
        vk::ShaderPtr           vs;
        vk::ShaderPtr           fs;
        vk::VertexInputPtr      vertexInput;
    };

} // namespace sky

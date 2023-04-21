//
// Created by Zach Lee on 2023/4/22.
//

#pragma once

#include <perf/render/Render.h>

namespace sky::perf {

    struct UIRes {
        vk::GraphicsPipelinePtr pso;
        vk::PipelineLayoutPtr   pipelineLayout;
        vk::VertexInputPtr      vertexInput;
        vk::DescriptorSetPtr    set;
        vk::DescriptorSetBinderPtr setBinder;
        vk::SamplerPtr          sampler;
        vk::VertexAssemblyPtr   assembler;
        vk::PushConstantsPtr    pushConstants;

        vk::ImagePtr fontImage;
        vk::ImageViewPtr fontImageView;

        vk::BufferPtr vb;
        vk::BufferPtr ib;
    };

    class Gui {
    public:
        Gui() = default;
        ~Gui();

        void Init();
        void Tick(float time);

    private:
        void InitFont();
        void InitPso();
        bool CreateOrResize(vk::BufferPtr &buffer, size_t size, bool vtx);
        vk::ShaderPtr InitShader(VkShaderStageFlagBits stage, const std::vector<uint32_t> &spv);

        std::unique_ptr<Render> render;
        std::unique_ptr<UIRes> res;
    };

} // namespace sky::perf
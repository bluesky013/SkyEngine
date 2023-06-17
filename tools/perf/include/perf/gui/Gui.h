//
// Created by Zach Lee on 2023/4/22.
//

#pragma once

#include <perf/render/Render.h>

namespace sky::perf {
    class ADB;

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

    class Gui : public IWindowEvent {
    public:
        Gui() = default;
        ~Gui();

        void Init();

        void Tick(float time, ADB& adb, std::function<void(ADB &)> &&);

    private:
        void InitFont();
        void InitPso();
        bool CreateOrResize(vk::BufferPtr &buffer, size_t size, bool vtx);

        void OnMouseWheel(int32_t wheelX, int32_t wheelY) override;
        void OnMouseMove(int32_t x, int32_t y) override;
        void OnMouseButtonDown(MouseButtonType button) override;
        void OnMouseButtonUp(MouseButtonType button) override;
        void OnKeyUp(KeyButtonType) override;
        void OnKeyDown(KeyButtonType) override;
        void OnTextInput(const char *text) override;

        vk::ShaderPtr InitShader(VkShaderStageFlagBits stage, const std::vector<uint32_t> &spv);

        std::unique_ptr<Render> render;
        std::unique_ptr<UIRes> res;

        vk::BufferPtr backVB;
        vk::BufferPtr backIB;
    };

} // namespace sky::perf
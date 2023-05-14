//
// Created by Zach Lee on 2022/6/16.
//

#include <EngineRoot.h>
#include <core/util/Memory.h>
#include <core/math/Vector4.h>
#include "VulkanBindlessSample.h"

namespace sky {

    struct ColorU8 {
        uint8_t color[4];
    };

    struct ImageInfo {
        VkFormat format;
        VkExtent3D extent;
        ColorU8 color;
    };

    struct Vertex {
        float offset[2];
        int id;
        int padding;
    };

    struct Material {
        Vector4 color;
        int texIndex[4] = {0};
    };

    ImageInfo g_infos[VulkanBindlessSample::IMAGE_NUM] = {
        {VK_FORMAT_R8G8B8A8_UNORM, {2, 2, 1}, {255, 0, 0, 255}},
        {VK_FORMAT_B8G8R8A8_UNORM, {4, 4, 1}, {0, 255, 0, 255}},
        {VK_FORMAT_R8G8B8A8_UNORM, {8, 8, 1}, {0, 0, 255, 255}},
        {VK_FORMAT_B8G8R8A8_UNORM, {16, 16, 1}, {255, 255, 255, 255}},
    };

    void VulkanBindlessSample::SetupPso()
    {
        vertexInput = vk::VertexInput::Builder().Begin()
                          .AddStream(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_INSTANCE)
                          .AddAttribute(0, 0, 0, VK_FORMAT_R32G32_SFLOAT)
                          .AddAttribute(1, 0, 8, VK_FORMAT_R32_SINT)
                          .Build();

        vk::GraphicsPipeline::Program program;
        vs = LoadShader(VK_SHADER_STAGE_VERTEX_BIT, ENGINE_ROOT + "/assets/shaders/output/Bindless.vert.spv");
        fs = LoadShader(VK_SHADER_STAGE_FRAGMENT_BIT, ENGINE_ROOT + "/assets/shaders/output/Bindless.frag.spv");
        program.shaders.emplace_back(vs);
        program.shaders.emplace_back(fs);

        vk::GraphicsPipeline::State state = {};
        state.raster.cullMode = VK_CULL_MODE_NONE;


        state.blends.blendStates.emplace_back(vk::GraphicsPipeline::BlendState{});
        state.blends.blendStates.back().blendEnable = VK_TRUE;
        state.blends.blendStates.back().srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        state.blends.blendStates.back().dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        state.blends.blendStates.back().srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        state.blends.blendStates.back().dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

        vk::GraphicsPipeline::VkDescriptor psoDesc = {};
        psoDesc.program                           = &program;
        psoDesc.state                             = &state;
        psoDesc.pipelineLayout                    = pipelineLayout;
        psoDesc.vertexInput                       = vertexInput;
        psoDesc.renderPass                        = renderPass;
        pso                                       = device->CreateDeviceObject<vk::GraphicsPipeline>(psoDesc);
    }

    void VulkanBindlessSample::SetupDescriptorSet()
    {
        vk::DescriptorSetLayout::VkDescriptor setLayoutInfo = {};
        setLayoutInfo.bindings.emplace(0, vk::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT});
        setLayoutInfo.bindings.emplace(1, vk::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT});
        setLayoutInfo.bindings.emplace(2,
                                       vk::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,IMAGE_NUM,VK_SHADER_STAGE_FRAGMENT_BIT, 0, VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT});

        vk::PipelineLayout::VkDescriptor pipelineLayoutInfo = {};
        pipelineLayoutInfo.desLayouts.emplace_back(setLayoutInfo);

        pipelineLayout = device->CreateDeviceObject<vk::PipelineLayout>(pipelineLayoutInfo);

        VkDescriptorPoolSize sizes[] = {
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
            {VK_DESCRIPTOR_TYPE_SAMPLER, 1},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, IMAGE_NUM},
        };

        vk::DescriptorSetPool::VkDescriptor poolInfo = {};
        poolInfo.maxSets                            = 1;
        poolInfo.num                                = 3;
        poolInfo.sizes                              = sizes;
        setPool                                     = device->CreateDeviceObject<vk::DescriptorSetPool>(poolInfo);
        set                                         = pipelineLayout->Allocate(setPool, 0);

        setBinder = std::make_shared<vk::DescriptorSetBinder>();
        setBinder->SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS);
        setBinder->SetPipelineLayout(pipelineLayout);
        setBinder->BindSet(0, set);
    }

    void VulkanBindlessSample::SetupResources()
    {
        auto  writer = set->CreateWriter();
        auto &limits = device->GetProperties().limits;

        sampler = device->CreateDeviceObject<vk::Sampler>(vk::Sampler::VkDescriptor{});
        writer.Write(1, VK_DESCRIPTOR_TYPE_SAMPLER, {}, sampler);

        auto cmd = device->GetGraphicsQueue()->AllocateCommandBuffer({});
        cmd->Begin();

        vk::Image::VkDescriptor imageInfo = {};
        imageInfo.memory                 = VMA_MEMORY_USAGE_GPU_ONLY;
        imageInfo.usage                  = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        std::vector<vk::BufferPtr> stagingBuffer;

        vk::ImageView::VkDescriptor viewInfo = {};
        for (uint32_t i = 0; i < IMAGE_NUM; ++i) {
            imageInfo.format = g_infos[i].format;
            imageInfo.extent = g_infos[i].extent;
            images[i]        = device->CreateDeviceObject<vk::Image>(imageInfo);

            std::vector<ColorU8> data(imageInfo.extent.width * imageInfo.extent.height, g_infos[i].color);

            vk::Buffer::VkDescriptor bufferInfo = {};
            bufferInfo.usage                   = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            bufferInfo.memory                  = VMA_MEMORY_USAGE_CPU_ONLY;
            bufferInfo.size                    = data.size() * sizeof(ColorU8);
            auto staging                       = device->CreateDeviceObject<vk::Buffer>(bufferInfo);
            stagingBuffer.emplace_back(staging);
            uint8_t *dst = staging->Map();
            memcpy(dst, data.data(), bufferInfo.size);
            staging->UnMap();

            VkBufferImageCopy copyInfo = {};
            copyInfo.bufferOffset      = 0;
            copyInfo.imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copyInfo.imageExtent       = imageInfo.extent;
            cmd->ImageBarrier(images[i], {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
                              {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT},
                              VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            cmd->Copy(staging, images[i], copyInfo);
            cmd->ImageBarrier(
                images[i], {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
                {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT},
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            viewInfo.format = imageInfo.format;
            imageViews[i]   = vk::ImageView::CreateImageView(images[i], viewInfo);
        }
        cmd->End();
        cmd->Submit(*graphicsQueue, {});
        cmd->Wait();

        for (uint32_t i = 0; i < IMAGE_NUM; ++i) {
            writer.Write(2, VK_DESCRIPTOR_TYPE_SAMPLER, imageViews[i], {}, i);
        }

        {
            std::vector<VkDrawIndirectCommand> commands = {
                {6, 1, 0, 0},
                {6, 1, 0, 1},
                {6, 1, 0, 2},
                {6, 1, 0, 3},
            };

            vk::Buffer::VkDescriptor bufferDesc = {};
            bufferDesc.size      = 4 * sizeof(VkDrawIndirectCommand);
            bufferDesc.usage     = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
            bufferDesc.memory    = VMA_MEMORY_USAGE_CPU_TO_GPU;
            indirectBuffer = device->CreateDeviceObject<vk::Buffer>(bufferDesc);

            uint8_t *ptr = indirectBuffer->Map();
            memcpy(ptr, commands.data(), commands.size() * sizeof(VkDrawIndirectCommand));
            indirectBuffer->UnMap();
        }

        {

            std::vector<Vertex> vertices = {
                {-0.5, -0.5, 0, 0},
                {-0.5,  0.5, 1, 0},
                { 0.5,  0.5, 2, 0},
                { 0.5, -0.5, 3, 0}};

            vk::Buffer::VkDescriptor bufferDesc = {};
            bufferDesc.size                    = 4 * sizeof(Vertex);
            bufferDesc.usage                   = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            bufferDesc.memory                  = VMA_MEMORY_USAGE_CPU_TO_GPU;
            vertexBuffer                       = device->CreateDeviceObject<vk::Buffer>(bufferDesc);

            uint8_t *ptr = vertexBuffer->Map();
            memcpy(ptr, vertices.data(), bufferDesc.size);
            vertexBuffer->UnMap();

            vertexAssembly = std::make_shared<vk::VertexAssembly>(*device);
            vertexAssembly->SetVertexInput(vertexInput);
            vertexAssembly->AddVertexBuffer(vk::BufferView::CreateBufferView(vertexBuffer, {}));
        }

        {
            std::vector<Material> materials = {
                {{0.5f, 0.5f, 0.5f, 1.f}, {0, 0, 0, 0}},
                {{0.6f, 0.6f, 0.6f, 1.f}, {1, 0, 0, 0}},
                {{0.7f, 0.7f, 0.7f, 1.f}, {2, 0, 0, 0}},
                {{0.7f, 0.7f, 0.7f, 1.f}, {3, 0, 0, 0}},
            };

            vk::Buffer::VkDescriptor bufferDesc = {};
            bufferDesc.size                    = 4 * sizeof(Material);
            bufferDesc.usage                   = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            bufferDesc.memory                  = VMA_MEMORY_USAGE_CPU_TO_GPU;
            materialBuffer                     = device->CreateDeviceObject<vk::Buffer>(bufferDesc);

            uint8_t *ptr = materialBuffer->Map();
            memcpy(ptr, materials.data(), bufferDesc.size);
            materialBuffer->UnMap();
            writer.Write(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, materialBuffer, 0, bufferDesc.size);
        }
        writer.Update();
    }


    void VulkanBindlessSample::OnTick(float delta)
    {
        uint32_t imageIndex = 0;
        swapChain->AcquireNext(imageAvailable, imageIndex);

        commandBuffer->Wait();
        commandBuffer->Begin();

        auto cmd             = commandBuffer->GetNativeHandle();
        auto graphicsEncoder = commandBuffer->EncodeVkGraphics();

        VkClearValue clearValue     = {};
        clearValue.color.float32[0] = 0.2f;
        clearValue.color.float32[1] = 0.2f;
        clearValue.color.float32[2] = 0.2f;
        clearValue.color.float32[3] = 1.f;

        vk::PassBeginInfo beginInfo = {};
        beginInfo.frameBuffer        = frameBuffers[imageIndex];
        beginInfo.renderPass         = renderPass;
        beginInfo.clearValueCount    = 1;
        beginInfo.clearValues        = &clearValue;

        graphicsEncoder.BeginPass(beginInfo);

        graphicsEncoder.BindPipeline(pso);
        graphicsEncoder.BindShaderResource(setBinder);
        graphicsEncoder.BindAssembly(vertexAssembly);
        graphicsEncoder.DrawIndirect(indirectBuffer, 0, 4 * sizeof(VkDrawIndirectCommand));

        graphicsEncoder.EndPass();

        commandBuffer->End();

        vk::CommandBuffer::SubmitInfo submitInfo = {};
        submitInfo.submitSignals.emplace_back(renderFinish);
        submitInfo.waits.emplace_back(
            std::pair<VkPipelineStageFlags, vk::SemaphorePtr>{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, imageAvailable});

        commandBuffer->Submit(*graphicsQueue, submitInfo);

        vk::SwapChain::PresentInfo presentInfo = {};
        presentInfo.imageIndex                  = imageIndex;
        presentInfo.signals.emplace_back(renderFinish);
        swapChain->Present(presentInfo);

        VulkanSampleBase::OnTick(delta);
    }

    void VulkanBindlessSample::OnStart()
    {
        VulkanSampleBase::OnStart();
        SetupDescriptorSet();
        SetupPso();
        SetupResources();
    }

    void VulkanBindlessSample::OnStop()
    {
        device->WaitIdle();
        pipelineLayout      = nullptr;
        descriptorSetLayout = nullptr;
        pso                 = nullptr;
        set                 = nullptr;
        setPool             = nullptr;
        setBinder           = nullptr;
        vs                  = nullptr;
        fs                  = nullptr;
        vertexInput         = nullptr;
        vertexAssembly      = nullptr;
        vertexBuffer        = nullptr;
        materialBuffer      = nullptr;
        indirectBuffer      = nullptr;
        sampler             = nullptr;

        for (uint32_t i = 0; i < IMAGE_NUM; ++i) {
            images[i]     = nullptr;
            imageViews[i] = nullptr;
        }

        VulkanSampleBase::OnStop();
    }

    void VulkanBindlessSample::InitFeature()
    {
        deviceInfo.feature.descriptorIndexing = true;
    }

} // namespace sky

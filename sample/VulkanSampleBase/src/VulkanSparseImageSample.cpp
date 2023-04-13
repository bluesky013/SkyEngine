//
// Created by Zach Lee on 2022/11/30.
//

#include <EngineRoot.h>
#include "VulkanSparseImageSample.h"

namespace sky {

    struct ColorU8 {
        uint8_t color[4];
    };

    void VulkanSparseImageSample::InitFeature()
    {
        deviceInfo.feature.sparseBinding = true;
    }

    void VulkanSparseImageSample::InitSparseImage()
    {
        vk::SparseImage::VkDescriptor imageDesc = {};
        imageDesc.imageType   = VK_IMAGE_TYPE_2D;
        imageDesc.format      = VK_FORMAT_R8G8B8A8_UNORM;
        imageDesc.extent      = {128 * 6, 128 * 6, 1};
        imageDesc.mipLevels   = 1;
        imageDesc.arrayLayers = 1;
        imageDesc.usage       = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageDesc.viewType    = VK_IMAGE_VIEW_TYPE_2D;

        sparseImage = device->CreateDeviceObject<vk::SparseImage>(imageDesc);
        sampler = device->CreateDeviceObject<vk::Sampler>(vk::Sampler::VkDescriptor{});

        vk::SparseImage::VkPageInfo pageInfo = {};
        pageInfo.offset = {0, 0, 0};
        pageInfo.extent = {128, 128, 1};
        pageInfo.level = 0;
        pageInfo.level = 0;

        auto *page1 = sparseImage->AddPage(pageInfo);

        pageInfo.offset = {128, 128, 0};
        pageInfo.extent = {256, 256, 1};
        pageInfo.level = 0;
        pageInfo.level = 0;
        auto *page2 = sparseImage->AddPage(pageInfo);

        pageInfo.offset = {384, 384, 0};
        pageInfo.extent = {384, 384, 1};
        pageInfo.level = 0;
        pageInfo.level = 0;
        auto *page3 = sparseImage->AddPage(pageInfo);
        sparseImage->UpdateBinding();

        std::vector<ColorU8> data1(128 * 128, {255, 0,   0, 255});
        std::vector<ColorU8> data2(256 * 256, {0, 255,   0, 255});
        std::vector<ColorU8> data3(384 * 384, {0,   0, 255, 255});

        vk::Buffer::VkDescriptor bufferInfo = {};
        bufferInfo.usage  = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.memory = VMA_MEMORY_USAGE_CPU_ONLY;
        bufferInfo.size   = (data1.size() + data2.size() + data3.size()) * sizeof(ColorU8);
        auto staging      = device->CreateDeviceObject<vk::Buffer>(bufferInfo);
        uint8_t *dst = staging->Map();
        uint64_t offset1 = data1.size() * sizeof(ColorU8);
        uint64_t offset2 = (data1.size() + data2.size()) * sizeof(ColorU8);
        memcpy(dst, data1.data(), offset1);
        memcpy(dst + offset1, data2.data(), data2.size() * sizeof(ColorU8));
        memcpy(dst + offset2, data3.data(), data3.size() * sizeof(ColorU8));
        staging->UnMap();

        auto cmd = device->GetGraphicsQueue()->AllocateCommandBuffer({});
        cmd->Begin();

        cmd->ImageBarrier(sparseImage->GetImage(), {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
                          {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT},
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy copyInfo = {};
        copyInfo.bufferOffset      = 0;
        copyInfo.imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copyInfo.imageOffset       = page1->offset;
        copyInfo.imageExtent       = page1->extent;
        cmd->Copy(staging, sparseImage->GetImage(), copyInfo);

        copyInfo.bufferOffset      = offset1;
        copyInfo.imageOffset       = page2->offset;
        copyInfo.imageExtent       = page2->extent;
        cmd->Copy(staging, sparseImage->GetImage(), copyInfo);

        copyInfo.bufferOffset      = offset2;
        copyInfo.imageOffset       = page3->offset;
        copyInfo.imageExtent       = page3->extent;
        cmd->Copy(staging, sparseImage->GetImage(), copyInfo);


        cmd->ImageBarrier(sparseImage->GetImage(), {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
            {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT},
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        cmd->End();
        cmd->Submit(*graphicsQueue, {});
        cmd->Wait();

        set->CreateWriter()
            .Write(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, sparseImage->GetImageView(), sampler, 0)
            .Update();
    }

    void VulkanSparseImageSample::SetupDescriptorSet()
    {
        vk::DescriptorSetLayout::VkDescriptor setLayoutInfo = {};
        setLayoutInfo.bindings.emplace(
            0, vk::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT});

        vk::PipelineLayout::VkDescriptor pipelineLayoutInfo = {};
        pipelineLayoutInfo.desLayouts.emplace_back(setLayoutInfo);

        pipelineLayout = device->CreateDeviceObject<vk::PipelineLayout>(pipelineLayoutInfo);

        VkDescriptorPoolSize sizes[] = {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
        };

        vk::DescriptorSetPool::VkDescriptor poolInfo = {};

        poolInfo.maxSets = 1;
        poolInfo.num     = 1;
        poolInfo.sizes   = sizes;
        setPool          = device->CreateDeviceObject<vk::DescriptorSetPool>(poolInfo);
        set              = pipelineLayout->Allocate(setPool, 0);

        setBinder = std::make_shared<vk::DescriptorSetBinder>();
        setBinder->SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS);
        setBinder->SetPipelineLayout(pipelineLayout);
        setBinder->BindSet(0, set);
    }

    void VulkanSparseImageSample::OnStart()
    {
        VulkanSampleBase::OnStart();
        vertexInput = vk::VertexInput::Builder().Begin().Build();

        SetupDescriptorSet();
        InitSparseImage();

        vk::GraphicsPipeline::Program program;
        vs = LoadShader(VK_SHADER_STAGE_VERTEX_BIT, ENGINE_ROOT + "/assets/shaders/output/Sparse.vert.spv");
        fs = LoadShader(VK_SHADER_STAGE_FRAGMENT_BIT, ENGINE_ROOT + "/assets/shaders/output/Sparse.frag.spv");
        program.shaders.emplace_back(vs);
        program.shaders.emplace_back(fs);

        vk::GraphicsPipeline::State state = {};
        state.raster.cullMode             = VK_CULL_MODE_NONE;
        state.blends.blendStates.emplace_back(vk::GraphicsPipeline::BlendState{});

        vk::GraphicsPipeline::VkDescriptor psoDesc = {};
        psoDesc.program                          = &program;
        psoDesc.state                            = &state;
        psoDesc.pipelineLayout                   = pipelineLayout;
        psoDesc.vertexInput                      = vertexInput;
        psoDesc.renderPass                       = renderPass;
        pso                                      = device->CreateDeviceObject<vk::GraphicsPipeline>(psoDesc);
    }

    void VulkanSparseImageSample::OnStop()
    {
        device->WaitIdle();
        pso            = nullptr;
        pipelineLayout = nullptr;
        vs             = nullptr;
        fs             = nullptr;
        vertexInput    = nullptr;
        sparseImage    = nullptr;
        sampler        = nullptr;
        set            = nullptr;
        setPool        = nullptr;
        setBinder      = nullptr;
        VulkanSampleBase::OnStop();
    }

    void VulkanSparseImageSample::OnTick(float delta)
    {
        uint32_t imageIndex = 0;
        swapChain->AcquireNext(imageAvailable, imageIndex);

        commandBuffer->Wait();
        commandBuffer->Begin();

        auto cmd             = commandBuffer->GetNativeHandle();
        auto graphicsEncoder = commandBuffer->EncodeVkGraphics();

        VkClearValue clearValue     = {};
        clearValue.color.float32[0] = 0.f;
        clearValue.color.float32[1] = 0.f;
        clearValue.color.float32[2] = 0.f;
        clearValue.color.float32[3] = 1.f;

        rhi::CmdDraw args          = {};
        args.type                 = rhi::CmdDrawType::LINEAR;
        args.linear.firstVertex   = 0;
        args.linear.firstInstance = 0;
        args.linear.vertexCount   = 6;
        args.linear.instanceCount = 1;

        vk::DrawItem item = {};
        item.pso          = pso;
        item.drawArgs     = args;

        vk::PassBeginInfo beginInfo = {};
        beginInfo.frameBuffer       = frameBuffers[imageIndex];
        beginInfo.renderPass        = renderPass;
        beginInfo.clearValueCount   = 1;
        beginInfo.clearValues       = &clearValue;

        graphicsEncoder.BeginPass(beginInfo);
        graphicsEncoder.BindShaderResource(setBinder);
        graphicsEncoder.Encode(item);
        graphicsEncoder.EndPass();

        commandBuffer->End();

        vk::CommandBuffer::SubmitInfo submitInfo = {};
        submitInfo.submitSignals.emplace_back(renderFinish);
        submitInfo.waits.emplace_back(
            std::pair<VkPipelineStageFlags, vk::SemaphorePtr>{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, imageAvailable});

        commandBuffer->Submit(*graphicsQueue, submitInfo);

        vk::SwapChain::PresentInfo presentInfo = {};
        presentInfo.imageIndex                 = imageIndex;
        presentInfo.signals.emplace_back(renderFinish);
        swapChain->Present(presentInfo);

        VulkanSampleBase::OnTick(delta);
    }
} // namespace sky

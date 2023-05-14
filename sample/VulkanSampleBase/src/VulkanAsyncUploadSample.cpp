//
// Created by Zach Lee on 2022/6/16.
//

#include "VulkanAsyncUploadSample.h"

#include <EngineRoot.h>
#include <core/util/Memory.h>
#include <core/math/Vector4.h>
#include <core/file/FileIO.h>
#include <core/logger/Logger.h>
#include <rhi/Decode.h>
#include <future>
#include <vulkan/Conversion.h>

namespace sky {

    struct ColorU8 {
        uint8_t value[4];
    };

    void VulkanAsyncUploadSample::SetupPso()
    {
        vertexInput = vk::VertexInput::Builder().Begin().Build();

        vk::GraphicsPipeline::Program program;
        vs = LoadShader(VK_SHADER_STAGE_VERTEX_BIT, ENGINE_ROOT + "/assets/shaders/output/AsyncUpload.vert.spv");
        fs = LoadShader(VK_SHADER_STAGE_FRAGMENT_BIT, ENGINE_ROOT + "/assets/shaders/output/AsyncUpload.frag.spv");
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

    void VulkanAsyncUploadSample::SetupDescriptorSet()
    {
        vk::DescriptorSetLayout::VkDescriptor setLayoutInfo = {};
        setLayoutInfo.bindings.emplace(0, vk::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT});

        vk::PipelineLayout::VkDescriptor pipelineLayoutInfo = {};
        pipelineLayoutInfo.desLayouts.emplace_back(setLayoutInfo);

        pipelineLayout = device->CreateDeviceObject<vk::PipelineLayout>(pipelineLayoutInfo);

        VkDescriptorPoolSize sizes[] =
            {
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
            };

        vk::DescriptorSetPool::VkDescriptor poolInfo = {};
        poolInfo.maxSets = 1;
        poolInfo.num = 1;
        poolInfo.sizes = sizes;
        setPool = device->CreateDeviceObject<vk::DescriptorSetPool>(poolInfo);
        set = pipelineLayout->Allocate(setPool, 0);

        object.setBinder = std::make_shared<vk::DescriptorSetBinder>();
        object.setBinder->SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS);
        object.setBinder->SetPipelineLayout(pipelineLayout);
        object.setBinder->BindSet(0, set);
    }

    static uint32_t Ceil(uint32_t v0, uint32_t v1) {
        return (v0 + v1 - 1) / v1;
    }

    void VulkanAsyncUploadSample::SetupResources()
    {
        uint8_t *ptr = nullptr;
        uint32_t size = 0;
        ReadBin(ENGINE_ROOT + "/assets/images/Logo.dds", ptr, size);

        vk::Image::Descriptor tmpDesc = {};
        std::vector<rhi::ImageUploadRequest> requests;
        ProcessDDS(ptr, size, tmpDesc, requests);

        vk::Image::VkDescriptor imageDesc = {};
        imageDesc.imageType   = vk::FromRHI(tmpDesc.imageType);
        imageDesc.format      = vk::FromRHI(tmpDesc.format);
        imageDesc.extent      = {tmpDesc.extent.width, tmpDesc.extent.height, tmpDesc.extent.depth};
        imageDesc.mipLevels   = tmpDesc.mipLevels;
        imageDesc.arrayLayers = tmpDesc.arrayLayers;

        imageDesc.usage  = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageDesc.memory = VMA_MEMORY_USAGE_GPU_ONLY;
        image            = device->CreateDeviceObject<vk::Image>(imageDesc);

        vk::ImageView::VkDescriptor viewDesc = {};
        viewDesc.format = imageDesc.format;
        view = vk::ImageView::CreateImageView(image, viewDesc);

        vk::Sampler::VkDescriptor samplerDesc = {};
        samplerDesc.minLod = 0.f;
        samplerDesc.maxLod = 13;
        sampler = device->CreateDeviceObject<vk::Sampler>(samplerDesc);
        auto writer = set->CreateWriter();
        writer.Write(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, view, sampler);
        writer.Update();

        device->GetTransferQueue()->UploadImage(image, requests);
        object.uploadHandle = device->GetTransferQueue()->CreateTask([ptr](){
            delete []ptr;
        });
    }

    void VulkanAsyncUploadSample::OnTick(float delta)
    {
        uint32_t imageIndex = 0;
        swapChain->AcquireNext(imageAvailable, imageIndex);

        commandBuffer->Wait();
        commandBuffer->Begin();

        auto *cmd             = commandBuffer->GetNativeHandle();
        auto  graphicsEncoder = commandBuffer->EncodeVkGraphics();

        VkClearValue clearValue     = {};
        clearValue.color.float32[0] = 0.2f;
        clearValue.color.float32[1] = 0.2f;
        clearValue.color.float32[2] = 0.2f;
        clearValue.color.float32[3] = 1.f;

        vk::PassBeginInfo beginInfo = {};
        beginInfo.frameBuffer       = frameBuffers[imageIndex];
        beginInfo.renderPass        = renderPass;
        beginInfo.clearValueCount   = 1;
        beginInfo.clearValues       = &clearValue;

        graphicsEncoder.BeginPass(beginInfo);

        rhi::CmdDrawLinear drawLinear = {};
        drawLinear.firstVertex       = 0;
        drawLinear.firstInstance     = 0;
        drawLinear.vertexCount       = 6;
        drawLinear.instanceCount     = 13;

        graphicsEncoder.BindPipeline(pso);

        if (device->GetTransferQueue()->HasComplete(object.uploadHandle)) {
            graphicsEncoder.BindShaderResource(object.setBinder);
            graphicsEncoder.DrawLinear(drawLinear);
        }

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

    void VulkanAsyncUploadSample::OnStart()
    {
        VulkanSampleBase::OnStart();
        SetupDescriptorSet();
        SetupPso();
        SetupResources();
    }

    void VulkanAsyncUploadSample::OnStop()
    {
        device->WaitIdle();

        pipelineLayout      = nullptr;
        descriptorSetLayout = nullptr;
        pso                 = nullptr;
        set                 = nullptr;
        setPool             = nullptr;
        vs                  = nullptr;
        fs                  = nullptr;
        vertexInput         = nullptr;
        sampler             = nullptr;
        view                = nullptr;
        image               = nullptr;
        object.setBinder    = nullptr;

        VulkanSampleBase::OnStop();
    }

} // namespace sky

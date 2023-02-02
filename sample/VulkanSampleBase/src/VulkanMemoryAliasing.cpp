//
// Created by Zach Lee on 2022/6/16.
//

#include "VulkanMemoryAliasing.h"

#include <EngineRoot.h>
#include <core/math/Vector4.h>
#include <core/util/Memory.h>
#include <future>
#include <random>

namespace sky {

    struct ColorU8 {
        uint8_t value[4];
    };

    void VulkanMemoryAliasing::SetupPso()
    {
        particleSystem->vertexInput = vk::VertexInput::Builder()
                                         .Begin()
                                         .AddAttribute(0, 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT)
                                         .AddStream(0, sizeof(ParticleRender), VK_VERTEX_INPUT_RATE_INSTANCE)
                                         .Build();

        fullScreenInput = vk::VertexInput::Builder().Begin().Build();

        vs = LoadShader(VK_SHADER_STAGE_VERTEX_BIT, ENGINE_ROOT + "/assets/shaders/output/VkMemoryParticle.vert.spv");
        fs = LoadShader(VK_SHADER_STAGE_FRAGMENT_BIT, ENGINE_ROOT + "/assets/shaders/output/VkMemoryParticle.frag.spv");

        fullScreenVs = LoadShader(VK_SHADER_STAGE_VERTEX_BIT, ENGINE_ROOT + "/assets/shaders/output/FullScreen.vert.spv");
        fullScreenFs = LoadShader(VK_SHADER_STAGE_FRAGMENT_BIT, ENGINE_ROOT + "/assets/shaders/output/VkMemoryFullscreen.frag.spv");

        compositeFS = LoadShader(VK_SHADER_STAGE_FRAGMENT_BIT, ENGINE_ROOT + "/assets/shaders/output/VkMemoryComposite.frag.spv");

        cs = LoadShader(VK_SHADER_STAGE_COMPUTE_BIT, ENGINE_ROOT + "/assets/shaders/output/VkMemoryParticle.comp.spv");

        vk::GraphicsPipeline::Program program;
        program.shaders.emplace_back(vs);
        program.shaders.emplace_back(fs);

        vk::GraphicsPipeline::State state = {};
        state.raster.cullMode             = VK_CULL_MODE_NONE;

        state.blends.blendStates.emplace_back(vk::GraphicsPipeline::BlendState{});
        state.blends.blendStates.back().blendEnable         = VK_TRUE;
        state.blends.blendStates.back().srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        state.blends.blendStates.back().dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        state.blends.blendStates.back().srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        state.blends.blendStates.back().dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

        vk::GraphicsPipeline::VkDescriptor psoDesc = {};
        psoDesc.program                          = &program;
        psoDesc.state                            = &state;
        psoDesc.pipelineLayout                   = gfxLayout;
        psoDesc.vertexInput                      = particleSystem->vertexInput;
        psoDesc.renderPass                       = renderPass;
        gfxPipeline                              = device->CreateDeviceObject<vk::GraphicsPipeline>(psoDesc);

        program.shaders.clear();
        program.shaders.emplace_back(fullScreenVs);
        program.shaders.emplace_back(fullScreenFs);

        psoDesc.program        = &program;
        psoDesc.state          = &state;
        psoDesc.pipelineLayout = gfxLayout;
        psoDesc.vertexInput    = fullScreenInput;
        psoDesc.renderPass     = renderPass;
        fullScreenPso          = device->CreateDeviceObject<vk::GraphicsPipeline>(psoDesc);

        program.shaders.clear();
        program.shaders.emplace_back(fullScreenVs);
        program.shaders.emplace_back(compositeFS);

        psoDesc.program        = &program;
        psoDesc.state          = &state;
        psoDesc.pipelineLayout = compositeLayout;
        psoDesc.vertexInput    = fullScreenInput;
        psoDesc.renderPass     = renderPass;
        compositePso           = device->CreateDeviceObject<vk::GraphicsPipeline>(psoDesc);

        vk::ComputePipeline::VkDescriptor compDesc = {};
        compDesc.shader                          = cs;
        compDesc.pipelineLayout                  = compLayout;
        compPipeline                             = device->CreateDeviceObject<vk::ComputePipeline>(compDesc);
    }

    void VulkanMemoryAliasing::SetupDescriptorSet()
    {
        VkDescriptorPoolSize sizes[] = {
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
        };

        vk::DescriptorSetPool::VkDescriptor poolInfo = {};
        poolInfo.maxSets                           = 4;
        poolInfo.num                               = sizeof(sizes) / sizeof(VkDescriptorPoolSize);
        poolInfo.sizes                             = sizes;
        setPool                                    = device->CreateDeviceObject<vk::DescriptorSetPool>(poolInfo);

        {
            vk::DescriptorSetLayout::VkDescriptor setLayoutInfo = {};
            setLayoutInfo.bindings.emplace(
                0, vk::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT});

            vk::PipelineLayout::VkDescriptor pipelineLayoutInfo = {};
            pipelineLayoutInfo.desLayouts.emplace_back(setLayoutInfo);
            gfxLayout     = device->CreateDeviceObject<vk::PipelineLayout>(pipelineLayoutInfo);
            gfxSet        = gfxLayout->Allocate(setPool, 0);
            fullScreenSet = gfxLayout->Allocate(setPool, 0);
        }

        {
            vk::DescriptorSetLayout::VkDescriptor setLayoutInfo = {};
            setLayoutInfo.bindings.emplace(
                0, vk::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT});
            setLayoutInfo.bindings.emplace(
                1, vk::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT});

            vk::PipelineLayout::VkDescriptor pipelineLayoutInfo = {};
            pipelineLayoutInfo.desLayouts.emplace_back(setLayoutInfo);
            compositeLayout = device->CreateDeviceObject<vk::PipelineLayout>(pipelineLayoutInfo);
            compositeSet    = compositeLayout->Allocate(setPool, 0);
        }

        {
            vk::DescriptorSetLayout::VkDescriptor setLayoutInfo = {};
            setLayoutInfo.bindings.emplace(0, vk::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT});
            setLayoutInfo.bindings.emplace(1, vk::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT});
            setLayoutInfo.bindings.emplace(2, vk::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT});

            vk::PipelineLayout::VkDescriptor pipelineLayoutInfo = {};
            pipelineLayoutInfo.desLayouts.emplace_back(setLayoutInfo);
            compLayout = device->CreateDeviceObject<vk::PipelineLayout>(pipelineLayoutInfo);
            compSet    = compLayout->Allocate(setPool, 0);
        }
    }

    void VulkanMemoryAliasing::SetupResources()
    {
        {
            vk::Image::VkDescriptor imageDesc = {};
            imageDesc.format                = VK_FORMAT_R8G8B8A8_UNORM;
            imageDesc.extent                = {128, 128, 1};
            imageDesc.usage                 = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            imageDesc.memory                = VMA_MEMORY_USAGE_GPU_ONLY;
            image                           = device->CreateDeviceObject<vk::Image>(imageDesc);

            uint32_t             imageSize = imageDesc.extent.width * imageDesc.extent.height;
            std::vector<ColorU8> data(imageSize);
            for (uint32_t i = 0; i < imageDesc.extent.width; ++i) {
                float u = static_cast<float>(i) / imageDesc.extent.width * 2.f - 1.f;
                for (uint32_t j = 0; j < imageDesc.extent.height; ++j) {
                    float v = static_cast<float>(j) / imageDesc.extent.height * 2.f - 1.f;
                    if ((u - 4 * v - 1) <= 0 && (u + 4 * v - 1) <= 0) {
                        auto &color    = data[i * imageDesc.extent.height + j];
                        color.value[0] = 255;
                        color.value[1] = 255;
                        color.value[2] = 255;
                        color.value[3] = 255;
                    }
                }
            }
            vk::Buffer::VkDescriptor bufferInfo = {};
            bufferInfo.usage                  = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            bufferInfo.memory                 = VMA_MEMORY_USAGE_CPU_ONLY;
            bufferInfo.size                   = data.size() * sizeof(ColorU8);
            auto     staging                  = device->CreateDeviceObject<vk::Buffer>(bufferInfo);
            uint8_t *dst                      = staging->Map();
            memcpy(dst, data.data(), bufferInfo.size);
            staging->UnMap();

            VkBufferImageCopy copyInfo = {};
            copyInfo.bufferOffset      = 0;
            copyInfo.imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copyInfo.imageExtent       = imageDesc.extent;

            auto cmd = device->GetGraphicsQueue()->AllocateCommandBuffer({});
            cmd->Begin();

            cmd->ImageBarrier(image, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
                              {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT},
                              VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            cmd->Copy(staging, image, copyInfo);
            cmd->ImageBarrier(
                image, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
                {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT},
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            cmd->End();
            cmd->Submit(*graphicsQueue, {});
            cmd->Wait();

            vk::ImageView::VkDescriptor viewDesc = {};
            viewDesc.format                    = VK_FORMAT_R8G8B8A8_UNORM;
            view                               = vk::ImageView::CreateImageView(image, viewDesc);
            sampler                            = device->CreateDeviceObject<vk::Sampler>(vk::Sampler::VkDescriptor{});

            gfxSet->CreateWriter().Write(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, view, sampler).Update();

            gfxBinder = std::make_shared<vk::DescriptorSetBinder>();
            gfxBinder->SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS);
            gfxBinder->SetPipelineLayout(gfxLayout);
            gfxBinder->BindSet(0, gfxSet);
        }

        {
            vk::Buffer::VkDescriptor descriptor = {};
            descriptor.size                   = PARTICLE_NUM * sizeof(Particle);
            descriptor.usage                  = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            descriptor.memory                 = VMA_MEMORY_USAGE_CPU_TO_GPU;
            particleSystem->input              = device->CreateDeviceObject<vk::Buffer>(descriptor);

            ResetParticlePool();

            descriptor.size    = sizeof(FrameData);
            descriptor.usage   = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            descriptor.memory  = VMA_MEMORY_USAGE_CPU_TO_GPU;
            particleSystem->ubo = device->CreateDeviceObject<vk::Buffer>(descriptor);
        }

        {
            vk::Image::VkDescriptor imageDesc = {};
            imageDesc.format                = swapChain->GetVkFormat();
            imageDesc.extent                = {swapChain->GetVkExtent().width, swapChain->GetVkExtent().height, 1};
            imageDesc.usage                 = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            imageDesc.memory                = VMA_MEMORY_USAGE_GPU_ONLY;
            rasterTarget                    = device->CreateDeviceObject<vk::Image>(imageDesc);

            vk::ImageView::VkDescriptor viewDesc = {};
            viewDesc.format                    = swapChain->GetVkFormat();
            rasterTargetView                   = vk::ImageView::CreateImageView(rasterTarget, viewDesc);

            vk::FrameBuffer::VkDescriptor fbDesc = {swapChain->GetVkExtent(), renderPass, {rasterTargetView}};
            rasterFb                           = device->CreateDeviceObject<vk::FrameBuffer>(fbDesc);
        }

        {
            VkMemoryRequirements   requirements1 = {};
            VkMemoryRequirements   requirements2 = {};
            vk::Buffer::VkDescriptor descriptor    = {};
            descriptor.memory                    = VMA_MEMORY_USAGE_CPU_TO_GPU;
            descriptor.size                      = PARTICLE_NUM * sizeof(Particle);
            descriptor.usage                     = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            descriptor.allocateMem               = false;
            particleSystem->output                = device->CreateDeviceObject<vk::Buffer>(descriptor);
            vkGetBufferMemoryRequirements(device->GetNativeHandle(), particleSystem->output->GetNativeHandle(), &requirements1);

            vk::Image::VkDescriptor imageDesc = {};
            imageDesc.format                = swapChain->GetVkFormat();
            imageDesc.extent                = {128, 128, 1};
            imageDesc.usage                 = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            imageDesc.memory                = VMA_MEMORY_USAGE_GPU_ONLY;
            imageDesc.allocateMem           = false;
            fullScreenTarget                = device->CreateDeviceObject<vk::Image>(imageDesc);
            vkGetImageMemoryRequirements(device->GetNativeHandle(), fullScreenTarget->GetNativeHandle(), &requirements2);

            VkMemoryRequirements finalMemReq = {};
            finalMemReq.size                 = std::max(requirements1.size, requirements2.size);
            finalMemReq.alignment            = std::max(requirements1.alignment, requirements2.alignment);
            finalMemReq.memoryTypeBits       = requirements1.memoryTypeBits & requirements2.memoryTypeBits;

            VmaAllocationCreateInfo allocCreateInfo = {};
            allocCreateInfo.preferredFlags          = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

            vmaAllocateMemory(device->GetAllocator(), &finalMemReq, &allocCreateInfo, &alloc, nullptr);
            vmaBindBufferMemory(device->GetAllocator(), alloc, particleSystem->output->GetNativeHandle());
            vmaBindImageMemory(device->GetAllocator(), alloc, fullScreenTarget->GetNativeHandle());

            vk::ImageView::VkDescriptor viewDesc = {};
            viewDesc.format                    = swapChain->GetVkFormat();
            fullScreenTargetView               = vk::ImageView::CreateImageView(fullScreenTarget, viewDesc);

            vk::FrameBuffer::VkDescriptor fbDesc = {VkExtent2D{128, 128}, renderPass, {fullScreenTargetView}};
            fullScreenFb                       = device->CreateDeviceObject<vk::FrameBuffer>(fbDesc);
        }

        {
            compSet->CreateWriter()
                .Write(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, particleSystem->ubo, 0, sizeof(FrameData))
                .Write(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, particleSystem->input, 0, PARTICLE_NUM * sizeof(Particle))
                .Write(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, particleSystem->output, 0, PARTICLE_NUM * sizeof(ParticleRender))
                .Update();

            compBinder = std::make_shared<vk::DescriptorSetBinder>();
            compBinder->SetBindPoint(VK_PIPELINE_BIND_POINT_COMPUTE);
            compBinder->SetPipelineLayout(compLayout);
            compBinder->BindSet(0, compSet);

            particleSystem->vertexAssembly = std::make_shared<vk::VertexAssembly>();
            particleSystem->vertexAssembly->SetVertexInput(particleSystem->vertexInput);
            particleSystem->vertexAssembly->AddVertexBuffer(particleSystem->output);
        }
    }

    void VulkanMemoryAliasing::SetupPass()
    {
        sampledPass = vk::RenderPassFactory()()
                          .AddSubPass()
                          .AddColor()
                          .Format(swapChain->GetVkFormat())
                          .Layout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                          .ColorOp(VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
                          .Samples(VK_SAMPLE_COUNT_1_BIT)
                          .AddDependency()
                          .SetLinkage(VK_SUBPASS_EXTERNAL, 0)
                          .SetBarrier({VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_MEMORY_READ_BIT,
                                       VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT})
                          .AddDependency()
                          .SetLinkage(0, VK_SUBPASS_EXTERNAL)
                          .SetBarrier({VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                       VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT})
                          .Create(*device);

        {
            fullScreenSet->CreateWriter().Write(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, rasterTargetView, sampler).Update();

            fullScreenSetBinder = std::make_shared<vk::DescriptorSetBinder>();
            fullScreenSetBinder->SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS);
            fullScreenSetBinder->SetPipelineLayout(gfxLayout);
            fullScreenSetBinder->BindSet(0, fullScreenSet);
        }

        {
            compositeSet->CreateWriter()
                .Write(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, rasterTargetView, sampler)
                .Write(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, fullScreenTargetView, sampler)
                .Update();

            compositeSetBinder = std::make_shared<vk::DescriptorSetBinder>();
            compositeSetBinder->SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS);
            compositeSetBinder->SetPipelineLayout(compositeLayout);
            compositeSetBinder->BindSet(0, compositeSet);
        }
    }

    void VulkanMemoryAliasing::OnKeyUp(KeyButtonType button)
    {
        if (button == KeyButton::KEY_ESCAPE) {
            ResetParticlePool();
        }
    }

    void VulkanMemoryAliasing::OnTick(float delta)
    {
        uint32_t imageIndex = 0;
        swapChain->AcquireNext(imageAvailable, imageIndex);
        commandBuffer->Wait();
        FrameData *data  = (FrameData *)(particleSystem->ubo->Map());
        data->frameIndex = frame;
        data->delta      = delta;
        particleSystem->ubo->UnMap();

        commandBuffer->Begin();
        auto cmd             = commandBuffer->GetNativeHandle();
        auto graphicsEncoder = commandBuffer->EncodeGraphics();

        // pass 1: Compute
        {
            graphicsEncoder.BindComputePipeline(compPipeline);
            graphicsEncoder.BindShaderResource(compBinder);
            graphicsEncoder.Dispatch(DISPATCH, 1, 1);
        }

        {
            vk::Barrier barrier   = {};
            barrier.srcStageMask  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            barrier.dstStageMask  = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
            commandBuffer->BufferBarrier(particleSystem->output, barrier, VK_WHOLE_SIZE, 0);
        }

        // pass 2: Raster
        {
            VkClearValue clearValue     = {};
            clearValue.color.float32[0] = 0.2f;
            clearValue.color.float32[1] = 0.2f;
            clearValue.color.float32[2] = 0.2f;
            clearValue.color.float32[3] = 0.f;

            vk::PassBeginInfo beginInfo = {};
            beginInfo.frameBuffer       = rasterFb;
            beginInfo.renderPass        = sampledPass;
            beginInfo.clearValueCount   = 1;
            beginInfo.clearValues       = &clearValue;

            graphicsEncoder.BeginPass(beginInfo);

            rhi::CmdDrawLinear drawLinear = {};
            drawLinear.firstVertex       = 0;
            drawLinear.firstInstance     = 0;
            drawLinear.vertexCount       = 6;
            drawLinear.instanceCount     = PARTICLE_NUM;

            graphicsEncoder.BindPipeline(gfxPipeline);
            graphicsEncoder.BindAssembly(particleSystem->vertexAssembly);
            graphicsEncoder.BindShaderResource(gfxBinder);
            graphicsEncoder.DrawLinear(drawLinear);

            graphicsEncoder.EndPass();
        }

        // pass 3: Fullscreen
        {
            VkClearValue clearValue     = {};
            clearValue.color.float32[0] = 0.0f;
            clearValue.color.float32[1] = 0.0f;
            clearValue.color.float32[2] = 0.0f;
            clearValue.color.float32[3] = 0.0f;

            vk::PassBeginInfo beginInfo = {};
            beginInfo.frameBuffer       = fullScreenFb;
            beginInfo.renderPass        = sampledPass;
            beginInfo.clearValueCount   = 1;
            beginInfo.clearValues       = &clearValue;

            graphicsEncoder.BeginPass(beginInfo);

            rhi::CmdDrawLinear drawLinear = {};
            drawLinear.firstVertex       = 0;
            drawLinear.firstInstance     = 0;
            drawLinear.vertexCount       = 3;

            graphicsEncoder.BindPipeline(fullScreenPso);
            graphicsEncoder.BindShaderResource(fullScreenSetBinder);
            graphicsEncoder.DrawLinear(drawLinear);

            graphicsEncoder.EndPass();
        }

        // pass 4: Composite && Present
        {
            VkClearValue clearValue     = {};
            clearValue.color.float32[0] = 0.0f;
            clearValue.color.float32[1] = 0.0f;
            clearValue.color.float32[2] = 0.0f;
            clearValue.color.float32[3] = 0.0f;

            vk::PassBeginInfo beginInfo = {};
            beginInfo.frameBuffer       = frameBuffers[imageIndex];
            beginInfo.renderPass        = renderPass;
            beginInfo.clearValueCount   = 1;
            beginInfo.clearValues       = &clearValue;

            graphicsEncoder.BeginPass(beginInfo);

            rhi::CmdDrawLinear drawLinear = {};
            drawLinear.firstVertex       = 0;
            drawLinear.firstInstance     = 0;
            drawLinear.vertexCount       = 3;

            graphicsEncoder.BindPipeline(compositePso);
            graphicsEncoder.BindShaderResource(compositeSetBinder);
            graphicsEncoder.DrawLinear(drawLinear);

            graphicsEncoder.EndPass();
        }

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

    void VulkanMemoryAliasing::ResetParticlePool()
    {
        std::vector<Particle> particles(PARTICLE_NUM);

        std::random_device              rd;
        std::mt19937                    gen(rd());
        std::normal_distribution<float> distrib(100, 25);

        for (uint32_t i = 0; i < PARTICLE_NUM; ++i) {
            particles[i].pos.x = 0;
            particles[i].pos.y = 0;

            float angle        = (i / static_cast<float>(PARTICLE_NUM) * 2 - 1.f) * 3.14f / 8.f + (90 / 180.f * 3.14f);
            particles[i].dir.x = distrib(gen) / 120.0f * cos(angle);
            particles[i].dir.y = distrib(gen) / 120.0f * sin(angle);
        }
        memcpy(particleSystem->input->Map(), particles.data(), particles.size() * sizeof(Particle));
        particleSystem->input->UnMap();
    }

    void VulkanMemoryAliasing::OnStart()
    {
        VulkanSampleBase::OnStart();

        particleSystem = std::make_unique<ParticleSystem>();
        SetupDescriptorSet();
        SetupPso();
        SetupResources();
        SetupPass();
    }

    void VulkanMemoryAliasing::OnStop()
    {
        device->WaitIdle();
        setPool              = nullptr;
        gfxLayout            = nullptr;
        gfxPipeline          = nullptr;
        compLayout           = nullptr;
        compPipeline         = nullptr;
        compositeLayout      = nullptr;
        compositePipeline    = nullptr;
        gfxSet               = nullptr;
        compSet              = nullptr;
        fullScreenSet        = nullptr;
        compositeSet         = nullptr;
        gfxBinder            = nullptr;
        compBinder           = nullptr;
        fullScreenSetBinder  = nullptr;
        compositeSetBinder   = nullptr;
        vs                   = nullptr;
        fs                   = nullptr;
        cs                   = nullptr;
        fullScreenVs         = nullptr;
        fullScreenFs         = nullptr;
        compositeFS          = nullptr;
        sampler              = nullptr;
        view                 = nullptr;
        image                = nullptr;
        sampledPass          = nullptr;
        rasterTarget         = nullptr;
        rasterTargetView     = nullptr;
        rasterFb             = nullptr;
        fullScreenTarget     = nullptr;
        fullScreenTargetView = nullptr;
        fullScreenFb         = nullptr;
        fullScreenPso        = nullptr;
        fullScreenInput      = nullptr;
        compositePso         = nullptr;
        particleSystem       = nullptr;

        vmaFreeMemory(device->GetAllocator(), alloc);
        VulkanSampleBase::OnStop();
    }

} // namespace sky

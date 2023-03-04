//
// Created by Zach Lee on 2022/6/16.
//

#include <EngineRoot.h>
#include <VulkanVariableRateShading.h>

namespace sky {

    void VulkanVariableRateShading::SetupRenderPass()
    {
        vk::RenderPass::VkDescriptor passDesc = {};
        passDesc.attachments.emplace_back(VkAttachmentDescription2{
            VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2, nullptr, 0,
            swapChain->GetVkFormat(),
            VK_SAMPLE_COUNT_1_BIT,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        });
        passDesc.attachments.emplace_back(VkAttachmentDescription2{
            VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2, nullptr, 0,
            VK_FORMAT_R8_UINT,
            VK_SAMPLE_COUNT_1_BIT,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR,
        });
        passDesc.subPasses.emplace_back(vk::RenderPass::SubPass{
            {{VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, nullptr, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT}}, {}, {},
            {VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, nullptr, ~(0U)},
            {VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, nullptr, 1, VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR, VK_IMAGE_ASPECT_COLOR_BIT}
        });

        passDesc.dependencies.emplace_back(VkSubpassDependency2 {
            VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2, nullptr,
            VK_SUBPASS_EXTERNAL, 0,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_DEPENDENCY_BY_REGION_BIT, 0
        });

        passDesc.dependencies.emplace_back(VkSubpassDependency2 {
            VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2, nullptr,
            0, VK_SUBPASS_EXTERNAL,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
            VK_DEPENDENCY_BY_REGION_BIT, 0
        });

        shadingRatePass = device->CreateDeviceObject<vk::RenderPass>(passDesc);
        SetupFrameBuffer();
    }

    void VulkanVariableRateShading::SetupFrameBuffer()
    {
        auto &ext = swapChain->GetVkExtent();
        vk::Image::VkDescriptor imageDesc = {};
        imageDesc.imageType   = VK_IMAGE_TYPE_2D;
        imageDesc.format      = VK_FORMAT_R8_UINT;
        imageDesc.extent      = {static_cast<uint32_t>(ceil(ext.width / static_cast<float>(16))),
                                 static_cast<uint32_t>(ceil(ext.height / static_cast<float>(16))),
                                 1};
        imageDesc.usage       = VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR | VK_IMAGE_USAGE_STORAGE_BIT;
        imageDesc.memory      = VMA_MEMORY_USAGE_GPU_ONLY;
        shadingRateImage = device->CreateDeviceObject<vk::Image>(imageDesc);

        shadingRateImageView = vk::ImageView::CreateImageView(shadingRateImage, vk::ImageView::VkDescriptor{VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8_UINT});

        vk::FrameBuffer::VkDescriptor fbDesc = {};
        fbDesc.extent = {ext.width, ext.height};
        fbDesc.pass = shadingRatePass;
        fbDesc.views.resize(2);
        fbDesc.views[1] = shadingRateImageView;

        uint32_t size = swapChain->GetImageCount();
        shadingRateFbs.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            fbDesc.views[0] = colorViews[i];
            shadingRateFbs[i] = device->CreateDeviceObject<vk::FrameBuffer>(fbDesc);
        }
    }

    void VulkanVariableRateShading::OnStart()
    {
        VulkanSampleBase::OnStart();

        SetupRenderPass();

        vertexInput = vk::VertexInput::Builder().Begin().Build();

        vk::PipelineLayout::VkDescriptor pDesc = {};
        pipelineLayout                         = device->CreateDeviceObject<vk::PipelineLayout>(pDesc);

        vk::GraphicsPipeline::Program program;
        vs = LoadShader(VK_SHADER_STAGE_VERTEX_BIT, ENGINE_ROOT + "/assets/shaders/output/VRS.vert.spv");
        fs = LoadShader(VK_SHADER_STAGE_FRAGMENT_BIT, ENGINE_ROOT + "/assets/shaders/output/VRS.frag.spv");
        program.shaders.emplace_back(vs);
        program.shaders.emplace_back(fs);

        vk::GraphicsPipeline::State state = {};
        state.raster.cullMode             = VK_CULL_MODE_NONE;
        state.blends.blendStates.emplace_back(vk::GraphicsPipeline::BlendState{});

        vk::GraphicsPipeline::VkDescriptor psoDesc = {};

        psoDesc.program        = &program;
        psoDesc.state          = &state;
        psoDesc.pipelineLayout = pipelineLayout;
        psoDesc.vertexInput    = vertexInput;
        psoDesc.renderPass     = shadingRatePass;

        state.vrs.combinerOps[0] = VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MAX_KHR;
        state.vrs.combinerOps[1] = VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MAX_KHR;
        pso1                   = device->CreateDeviceObject<vk::GraphicsPipeline>(psoDesc);

        state.vrs.fragmentSize = {4, 4};
        state.vrs.combinerOps[0] = VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MAX_KHR;
        state.vrs.combinerOps[1] = VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MAX_KHR;
        pso2                     = device->CreateDeviceObject<vk::GraphicsPipeline>(psoDesc);
    }

    void VulkanVariableRateShading::OnStop()
    {
        device->WaitIdle();
        shadingRateImage = nullptr;
        shadingRateImageView = nullptr;
        shadingRatePass = nullptr;
        shadingRateFbs.clear();

        pso1           = nullptr;
        pso2           = nullptr;
        pipelineLayout = nullptr;
        vs             = nullptr;
        fs             = nullptr;
        vertexInput    = nullptr;
        VulkanSampleBase::OnStop();
    }

    void VulkanVariableRateShading::OnTick(float delta)
    {
        uint32_t imageIndex = 0;
        swapChain->AcquireNext(imageAvailable, imageIndex);

        commandBuffer->Wait();
        commandBuffer->Begin();

        auto cmd             = commandBuffer->GetNativeHandle();
        auto graphicsEncoder = commandBuffer->EncodeVkGraphics();

        std::vector<VkClearValue> clears(2);
        clears[0].color.float32[0] = 0.f;
        clears[0].color.float32[1] = 0.f;
        clears[0].color.float32[2] = 0.f;
        clears[0].color.float32[3] = 0.f;

        clears[1].color.uint32[0] = 15;
        clears[1].color.uint32[1] = 15;
        clears[1].color.uint32[2] = 15;
        clears[1].color.uint32[3] = 15;

        rhi::CmdDraw args          = {};
        args.type                 = rhi::CmdDrawType::LINEAR;
        args.linear.firstVertex   = 0;
        args.linear.firstInstance = 0;
        args.linear.vertexCount   = 3;
        args.linear.instanceCount = 1;

        vk::PassBeginInfo beginInfo = {};
        beginInfo.frameBuffer       = shadingRateFbs[imageIndex];
        beginInfo.renderPass        = shadingRatePass;
        beginInfo.clearValueCount   = 2;
        beginInfo.clearValues       = clears.data();

        auto &ext = swapChain->GetVkExtent();
        VkViewport viewport = {};
        viewport.x = 0.f;
        viewport.y = 0.f;
        viewport.width = ext.width / 2.f;
        viewport.height = ext.height / 1.0f;
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;

        graphicsEncoder.BeginPass(beginInfo);
        graphicsEncoder.BindPipeline(pso1);
        graphicsEncoder.SetViewport(1, &viewport);
        graphicsEncoder.DrawLinear(args.linear);

        viewport.x = ext.width / 2.f;
        graphicsEncoder.BindPipeline(pso2);
        graphicsEncoder.SetViewport(1, &viewport);
        graphicsEncoder.DrawLinear(args.linear);
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

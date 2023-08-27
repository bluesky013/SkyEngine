//
// Created by Zach Lee on 2023/5/30.
//

#include <RHIDrawIndirectSample.h>
#include <builder/shader/ShaderCompiler.h>
#include <core/environment/Singleton.h>
#include <core/math/Vector2.h>
#include <framework/platform/PlatformBase.h>

//#define ENABLE_MULTI_DRAW

namespace sky::rhi {

    const uint32_t X_NUM = 8;
    const uint32_t Y_NUM = 8;
    const uint32_t TOTAL_NUM = X_NUM * Y_NUM;

    struct InstanceData {
        Vector2 offset;
        Vector2 scale;
    };

    struct IndirectCommand {
        uint32_t vertexCount;
        uint32_t instanceCount;
        uint32_t firstVertex;
        uint32_t firstInstance;
    };

    void RHIDrawIndirectSample::SetFeature()
    {
#ifdef ENABLE_MULTI_DRAW
        deviceFeature.multiDrawIndirect = true;
#else
        deviceFeature.multiDrawIndirect = false;
#endif
    }

    void RHIDrawIndirectSample::SetupBase()
    {
        SetupPass();
        SetupPool();

        rhi::VertexInput::Descriptor viDesc = {};
        viDesc.attributes.emplace_back(VertexAttributeDesc {0, 0, 0, Format::F_RG32});
        viDesc.attributes.emplace_back(VertexAttributeDesc {1, 0, 8, Format::F_RG32});
        viDesc.bindings.emplace_back(VertexBindingDesc {0, sizeof(InstanceData), VertexInputRate::PER_INSTANCE});
        vi = device->CreateVertexInput(viDesc);

        auto path = Platform::Get()->GetInternalPath();
        builder::ShaderCompiler::CompileShader("shaders/triangle_instanced_vs.glsl", {path + "/shaders/RHISample/triangle_instanced_vs.shader", builder::ShaderType::VS});
        builder::ShaderCompiler::CompileShader("shaders/triangle_fs.glsl", {path + "/shaders/RHISample/triangle_fs.shader", builder::ShaderType::FS});
        GraphicsPipeline::Descriptor psoDesc = {};
        psoDesc.vs = CreateShader(rhi, *device, ShaderStageFlagBit::VS, path + "/shaders/RHISample/triangle_instanced_vs.shader");
        psoDesc.fs = CreateShader(rhi, *device, ShaderStageFlagBit::FS, path + "/shaders/RHISample/triangle_fs.shader");
        psoDesc.renderPass = renderPass;
        psoDesc.pipelineLayout = pipelineLayout;
        psoDesc.vertexInput = vi;
        psoDesc.state.depthStencil.depthTest = true;
        psoDesc.state.depthStencil.depthWrite = true;
        psoDesc.state.blendStates.emplace_back(BlendState{});
        pso = device->CreateGraphicsPipeline(psoDesc);

        rhi::Buffer::Descriptor bufferDesc = {};
        bufferDesc.size = sizeof(InstanceData) * TOTAL_NUM;
        bufferDesc.usage = BufferUsageFlagBit::VERTEX;
        bufferDesc.memory = MemoryType::CPU_TO_GPU;
        auto buffer = device->CreateBuffer(bufferDesc);
        instanceBuffer = buffer->CreateView({0, bufferDesc.size});

        std::vector<InstanceData> data(TOTAL_NUM);
        for (uint32_t i = 0; i < X_NUM; ++i) {
            for (uint32_t j = 0; j < Y_NUM; ++j) {
                auto &dat = data[i * Y_NUM + j];
                dat.scale = Vector2{1 / static_cast<float>(X_NUM), 1 / static_cast<float>(Y_NUM)};
                dat.offset.x = i * 2 / static_cast<float>(X_NUM) - 0.9f;
                dat.offset.y = j * 2 / static_cast<float>(Y_NUM) - 0.9f;
            }
        }
        auto *ptr = buffer->Map();
        memcpy(ptr, data.data(), bufferDesc.size);
        buffer->UnMap();

        rhi::VertexAssembly::Descriptor vaDesc = {};
        vaDesc.vertexBuffers.emplace_back(instanceBuffer);
        vaDesc.vertexInput = vi;
        va = device->CreateVertexAssembly(vaDesc);

        std::vector<IndirectCommand> indirect;
        for (uint32_t i = 0; i < Y_NUM; ++i) {
            indirect.emplace_back(IndirectCommand{3, 8, 0, i * X_NUM});
        }
        bufferDesc.size = sizeof(IndirectCommand) * indirect.size();
        bufferDesc.usage = BufferUsageFlagBit::INDIRECT;
        bufferDesc.memory = MemoryType::CPU_TO_GPU;
        indirectBuffer = device->CreateBuffer(bufferDesc);
        ptr = indirectBuffer->Map();
        memcpy(ptr, indirect.data(), bufferDesc.size);
        indirectBuffer->UnMap();
    }

    void RHIDrawIndirectSample::OnTick(float delta)
    {
        auto *queue = device->GetQueue(QueueType::GRAPHICS);
        uint32_t index = swapChain->AcquireNextImage(imageAvailable);

        SubmitInfo submitInfo = {};
        submitInfo.submitSignals.emplace_back(renderFinish);
        submitInfo.waits.emplace_back(PipelineStageBit::COLOR_OUTPUT, imageAvailable);
        submitInfo.fence = fence;

        fence->WaitAndReset();
        commandBuffer->Begin();

        {
            ImageBarrier barrier = {};
            barrier.srcFlags = rhi::AccessFlagBit::NONE;
            barrier.dstFlags = rhi::AccessFlagBit::COLOR_WRITE;
            barrier.view = colorViews[index];
            commandBuffer->QueueBarrier(barrier);
        }

        {
            ImageBarrier barrier = {};
            barrier.srcFlags = rhi::AccessFlagBit::NONE;
            barrier.dstFlags = rhi::AccessFlagBit::DEPTH_STENCIL_WRITE;
            barrier.view = depthStencilImage;
            commandBuffer->QueueBarrier(barrier);
        }

        commandBuffer->FlushBarriers();
        commandBuffer->EncodeGraphics()->BeginPass({frameBuffers[index], renderPass, 2, clears.data()})
            .BindPipeline(pso)
            .BindAssembly(va)
            .DrawIndirect(indirectBuffer, 0, X_NUM, sizeof(IndirectCommand))
            .EndPass();

        {
            ImageBarrier barrier = {};
            barrier.srcFlags = rhi::AccessFlagBit::COLOR_WRITE;
            barrier.dstFlags = rhi::AccessFlagBit::PRESENT;
            barrier.view = colorViews[index];
            commandBuffer->QueueBarrier(barrier);
            commandBuffer->FlushBarriers();
        }

        commandBuffer->End();
        commandBuffer->Submit(*queue, submitInfo);

        PresentInfo presentInfo = {};
        presentInfo.imageIndex = index;
        presentInfo.semaphores.emplace_back(renderFinish);
        swapChain->Present(*queue, presentInfo);
    }

    void RHIDrawIndirectSample::OnStop()
    {
        device->WaitIdle();
        vi = nullptr;
        va = nullptr;
        instanceBuffer = nullptr;
        indirectBuffer = nullptr;
        RHISampleBase::OnStop();
    }
} // namespace sky::rhi
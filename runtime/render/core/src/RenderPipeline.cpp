//
// Created by Zach Lee on 2023/8/20.
//

#include <render/RenderPipeline.h>

#include <sstream>

#include <render/RHI.h>
#include <render/rdg/TransientObjectPool.h>
#include <render/rdg/TransientMemoryPool.h>

#include <render/rdg/RenderGraphVisitors.h>
#include <render/rdg/RenderGraphExecutor.h>
#include <render/rdg/AccessGraphCompiler.h>
#include <render/rdg/RenderResourceCompiler.h>
#include <render/rdg/RenderSceneVisitor.h>
#include <render/rdg/RenderGraphUploader.h>
#include <render/Renderer.h>

#include <core/profile/Profiler.h>

namespace sky {

    RenderPipeline::~RenderPipeline() = default;

    RenderPipeline::RenderPipeline()
    {
        const auto &defaultRes = Renderer::Get()->GetDefaultRHIResource();

        rdgContext = std::make_unique<rdg::RenderGraphContext>();
        rdgContext->pool = std::make_unique<rdg::TransientObjectPool>();
        rdgContext->pool->Init();
        rdgContext->device = RHI::Get()->GetDevice();
        rdgContext->emptySet = defaultRes.emptySet;

        frameIndex = 0;
        inflightFrameCount = Renderer::Get()->GetInflightFrameCount();
        rdgContext->fences.resize(inflightFrameCount);
        rdgContext->commandBuffers.resize(inflightFrameCount);
        rdgContext->renderFinishSemaphores.resize(inflightFrameCount);
        rdgContext->imageAvailableSemaPools.resize(inflightFrameCount);

        rhi::Fence::Descriptor fenceDesc = {};
        fenceDesc.createSignaled = true;
        for (uint32_t i = 0; i <inflightFrameCount; ++i) {
            rdgContext->commandBuffers[i] = rdgContext->device->CreateCommandBuffer({});
            rdgContext->fences[i] = rdgContext->device->CreateFence(fenceDesc);
            rdgContext->renderFinishSemaphores[i] = rdgContext->device->CreateSema({});
        }
    }

    void RenderPipeline::FrameSync()
    {
        SKY_PROFILE_NAME("FrameSync");
        rdgContext->frameIndex = frameIndex;
        rdgContext->Fence()->WaitAndReset();
        rdgContext->ImageAvailableSemaPool().Reset();
        rdgContext->pool->ResetPool();
    }

    void RenderPipeline::Compile(rdg::RenderGraph &rdg)
    {
        using namespace rdg;
        {
            SKY_PROFILE_NAME("AccessCompiler");
            AccessCompiler             compiler(rdg);
            PmrVector<boost::default_color_type> colors(rdg.accessGraph.vertices.size(), &rdg.context->resources);
            boost::depth_first_search(rdg.accessGraph.graph, compiler, ColorMap(colors));
        }

        {
            SKY_PROFILE_NAME("RenderResourceCompiler");
            RenderResourceCompiler               compiler(rdg);
            PmrVector<boost::default_color_type> colors(rdg.vertices.size(), &rdg.context->resources);
            boost::depth_first_search(rdg.graph, compiler, ColorMap(colors));
        }

        {
            SKY_PROFILE_NAME("RenderGraphPassCompiler");
            RenderGraphPassCompiler              compiler(rdg);
            PmrVector<boost::default_color_type> colors(rdg.vertices.size(), &rdg.context->resources);
            boost::depth_first_search(rdg.graph, compiler, ColorMap(colors));
        }
    }

    void RenderPipeline::Collect(rdg::RenderGraph &rdg, const std::vector<RenderScene*> &scenes)
    {
        using namespace rdg;
        SKY_PROFILE_NAME("RenderSceneVisitor");
        for (auto *scene : scenes) {
            RenderSceneVisitor sceneVisitor(rdg, scene);
            sceneVisitor.BuildRenderQueue();
        }
    }

    void RenderPipeline::Execute(rdg::RenderGraph &rdg)
    {
        using namespace rdg;
        const auto &commandBuffer = rdgContext->MainCommandBuffer();
        auto *queue = rdgContext->device->GetQueue(rhi::QueueType::GRAPHICS);
        {
            SKY_PROFILE_NAME("Encode");

            commandBuffer->Begin();

            RenderGraphUploader uploader(rdg);
            uploader.UploadConstantBuffers();

            RenderGraphExecutor executor(rdg);
            PmrVector<boost::default_color_type> colors(rdg.vertices.size(), &rdg.context->resources);
            boost::depth_first_search(rdg.graph, executor, ColorMap(colors));
            commandBuffer->End();

            rhi::SubmitInfo submitInfo = {};
            submitInfo.submitSignals.emplace_back(rdgContext->RenderFinishSemaphore());

            auto &semaPool = rdgContext->ImageAvailableSemaPool();
            for (uint32_t i = 0; i < semaPool.index; ++i) {
                submitInfo.waits.emplace_back(rhi::PipelineStageBit::COLOR_OUTPUT, semaPool.imageAvailableSemaList[i]);
            }
            submitInfo.fence = rdgContext->Fence();
            commandBuffer->Submit(*queue, submitInfo);
        }

        {
            rhi::PresentInfo presentInfo = {};
            presentInfo.semaphores.emplace_back(rdgContext->RenderFinishSemaphore());
            for (auto &swc : rdg.presentPasses) {
#ifdef SKY_ENABLE_XR
                if (swc.xrSwapChain) {
                    continue;
                }
#endif
                auto &res = rdg.resourceGraph.swapChains[Index(swc.imageID, rdg.resourceGraph)];
                presentInfo.imageIndex = res.desc.imageIndex;
                swc.swapChain->Present(*queue, presentInfo);
            }
        }

        {
            auto *gc = Renderer::Get()->GetResourceGC();
            for (auto &res : rdg.resourceGraph.importImages) {
                gc->CollectImageViews(res.res);
            }
        }

        frameIndex = (frameIndex + 1) % inflightFrameCount;
    }

    std::string GetDefaultSceneViewUBOName(const SceneView &view)
    {
//        std::stringstream ss;
//        ss << "DEFAULT_VIEW_UBO_" << view.GetViewID();
//        return ss.str();
        return "DEFAULT_VIEW_UBO_0";
    }

} // namespace sky

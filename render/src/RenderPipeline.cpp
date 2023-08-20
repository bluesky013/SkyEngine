//
// Created by Zach Lee on 2023/8/20.
//

#include <render/RenderPipeline.h>
#include <render/RHI.h>
#include <render/rdg/TransientObjectPool.h>
#include <render/rdg/TransientMemoryPool.h>

#include <render/rdg/RenderGraphVisitors.h>
#include <render/rdg/RenderGraphExecutor.h>
#include <render/rdg/AccessGraphCompiler.h>
#include <render/rdg/RenderResourceCompiler.h>

namespace sky {

    RenderPipeline::RenderPipeline()
    {
        rdgContext = std::make_unique<rdg::RenderGraphContext>();
        rdgContext->pool = std::make_unique<rdg::TransientObjectPool>();
        rdgContext->device = RHI::Get()->GetDevice();
        rdgContext->mainCommandBuffer = rdgContext->device->CreateCommandBuffer({});

        rhi::Fence::Descriptor fenceDesc = {};
        fenceDesc.createSignaled = true;
        rdgContext->fence = rdgContext->device->CreateFence(fenceDesc);
        rdgContext->imageAvailable = rdgContext->device->CreateSema({});
        rdgContext->renderFinish = rdgContext->device->CreateSema({});
    }

    void RenderPipeline::Execute(rdg::RenderGraph &rdg)
    {
        using namespace rdg;
        {
            AccessCompiler             compiler(rdg);
            PmrVector<boost::default_color_type> colors(rdg.accessGraph.vertices.size(), &rdg.context->resources);
            boost::depth_first_search(rdg.accessGraph.graph, compiler, ColorMap(colors));
        }


        {
            RenderResourceCompiler               compiler(rdg);
            PmrVector<boost::default_color_type> colors(rdg.vertices.size(), &rdg.context->resources);
            boost::depth_first_search(rdg.graph, compiler, ColorMap(colors));
        }

        {
            RenderGraphPassCompiler              compiler(rdg);
            PmrVector<boost::default_color_type> colors(rdg.vertices.size(), &rdg.context->resources);
            boost::depth_first_search(rdg.graph, compiler, ColorMap(colors));
        }

        {
            rdgContext->fence->WaitAndReset();
            rdgContext->mainCommandBuffer->Begin();
            RenderGraphExecutor executor(rdg);
            PmrVector<boost::default_color_type> colors(rdg.vertices.size(), &rdg.context->resources);
            boost::depth_first_search(rdg.graph, executor, ColorMap(colors));
            rdgContext->mainCommandBuffer->End();

            auto *queue = rdgContext->device->GetQueue(rhi::QueueType::GRAPHICS);

            rhi::SubmitInfo submitInfo = {};
            submitInfo.submitSignals.emplace_back(rdgContext->renderFinish);
            submitInfo.waits.emplace_back(rhi::PipelineStageBit::COLOR_OUTPUT, rdgContext->imageAvailable);
            submitInfo.fence = rdgContext->fence;
            rdgContext->mainCommandBuffer->Submit(*queue, submitInfo);
        }
    }

} // namespace sky
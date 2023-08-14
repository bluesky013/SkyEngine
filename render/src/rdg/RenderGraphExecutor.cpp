//
// Created by Zach Lee on 2023/3/27.
//

#include <render/rdg/RenderGraphExecutor.h>

namespace sky::rdg {

    void RenderGraphExecutor::Barriers(const PmrHashMap<VertexType, GraphBarrier>& barriers) const
    {
        for (const auto &[first, second] : barriers) {
            const auto resID = first;
            const auto &barrier = second;
            std::visit(Overloaded{
                [&](const ImageTag &) {
                    auto &image = graph.resourceGraph.images[Index(resID, graph.resourceGraph)];
                    graph.context->mainCommandBuffer->QueueBarrier(image.desc.image,
                        rhi::ImageSubRange{static_cast<uint32_t>(barrier.range.base), static_cast<uint32_t>(barrier.range.range), barrier.range.layer, barrier.range.layers, barrier.range.aspectMask},
                        rhi::BarrierInfo{ barrier.srcFlags, barrier.dstFlags});
                },
                [&](const ImportImageTag &) {
                    auto &image = graph.resourceGraph.importImages[Index(resID, graph.resourceGraph)];
                    graph.context->mainCommandBuffer->QueueBarrier(image.desc.image,
                        rhi::ImageSubRange{static_cast<uint32_t>(barrier.range.base), static_cast<uint32_t>(barrier.range.range), barrier.range.layer, barrier.range.layers, barrier.range.aspectMask},
                        rhi::BarrierInfo{ barrier.srcFlags, barrier.dstFlags});
                },
                [&](const BufferTag &) {
                    auto &buffer = graph.resourceGraph.buffers[Index(resID, graph.resourceGraph)];
                    graph.context->mainCommandBuffer->QueueBarrier(buffer.desc.buffer, barrier.range.base, barrier.range.range,
                        rhi::BarrierInfo{ barrier.srcFlags, barrier.dstFlags});
                },
                [&](const ImportBufferTag &) {
                    auto &buffer = graph.resourceGraph.importBuffers[Index(resID, graph.resourceGraph)];
                    graph.context->mainCommandBuffer->QueueBarrier(buffer.desc.buffer, barrier.range.base, barrier.range.range,
                        rhi::BarrierInfo{ barrier.srcFlags, barrier.dstFlags});
                },
                [&](const auto &) {}
            }, Tag(resID, graph.resourceGraph));
        }
        graph.context->mainCommandBuffer->FlushBarriers();
    }

    [[maybe_unused]] void RenderGraphExecutor::discover_vertex(Vertex u, const Graph& g)
    {
        std::visit(Overloaded{
            [&](const RasterPassTag &) {
                auto &raster = graph.rasterPasses[Index(u, graph)];
                Barriers(raster.frontBarriers);

                rhi::PassBeginInfo beginInfo = {};
                beginInfo.frameBuffer = raster.frameBuffer;
                beginInfo.renderPass  = raster.renderPass;
                beginInfo.clearCount  = static_cast<uint32_t>(raster.clearValues.size());
                beginInfo.clearValues = raster.clearValues.data();

                currentEncoder = graph.context->mainCommandBuffer->EncodeGraphics();
                currentEncoder->BeginPass(beginInfo);

                // queue
                currentSubPassIndex = 0;
                currentSubPassNum = beginInfo.renderPass->GetSubPassNum();
            },
            [&](const RasterSubPassTag &) {
                auto &rasterSub = graph.subPasses[Index(u, graph)];

                ++currentSubPassIndex;
                if (currentSubPassIndex < currentSubPassNum) {
                    currentEncoder->NextSubPass();
                }
            },
            [&](const ComputePassTag &) {
                auto &compute = graph.computePasses[Index(u, graph)];
                Barriers(compute.frontBarriers);
                Barriers(compute.rearBarriers);
            },
            [&](const CopyBlitTag &) {
                auto &cb = graph.copyBlitPasses[Index(u, graph)];
                Barriers(cb.frontBarriers);
                Barriers(cb.rearBarriers);
            },
            [&](const PresentTag &) {
                auto &present = graph.presentPasses[Index(u, graph)];
                Barriers(present.frontBarriers);
                Barriers(present.rearBarriers);
            },
            [&](const RasterSceneViewTag &) {
                auto &view = graph.sceneViews[Index(u, graph)];

//                currentEncoder->BindPipeline(nullptr);
//                currentEncoder->BindSet(0, nullptr);
//                currentEncoder->BindSet(1, nullptr);
//                currentEncoder->BindSet(2, nullptr);
//                currentEncoder->BindAssembly(nullptr);
//                currentEncoder->DrawLinear({});
            },
            [&](const auto &) {}
        }, Tag(u, graph));
    }

    [[maybe_unused]] void RenderGraphExecutor::finish_vertex(Vertex u, const Graph& g)
    {
        std::visit(Overloaded{
            [&](const RasterPassTag &) {
                auto &raster = graph.rasterPasses[Index(u, graph)];
                currentEncoder->EndPass();
                Barriers(raster.rearBarriers);
            },
            [&](const ComputePassTag &) {
                auto &compute = graph.computePasses[Index(u, graph)];
                Barriers(compute.rearBarriers);
            },
            [&](const CopyBlitTag &) {
                auto &cb = graph.copyBlitPasses[Index(u, graph)];
                Barriers(cb.rearBarriers);
            },
            [&](const PresentTag &) {
                auto &present = graph.presentPasses[Index(u, graph)];
                Barriers(present.rearBarriers);
            },
            [&](const auto &) {}
        }, Tag(u, graph));
    }

}

//
// Created by Zach Lee on 2023/3/27.
//

#include <render/rdg/RenderGraphExecutor.h>

namespace sky::rdg {

    void RenderGraphExecutor::FrontBarriers(Vertex u, const Graph& g)
    {
        graph.context->mainCommandBuffer->FlushBarriers();
    }

    void RenderGraphExecutor::RearBarriers(Vertex u, const Graph& g)
    {
        graph.context->mainCommandBuffer->FlushBarriers();
    }

    void RenderGraphExecutor::discover_vertex(Vertex u, const Graph& g)
    {
        std::visit(Overloaded{
            [&](const RasterPassTag &) {
                auto &raster = graph.rasterPasses[Index(u, graph)];

                FrontBarriers(u, g);

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

                ++currentSubPassNum;
                if (currentSubPassNum <  currentSubPassNum) {
                    currentEncoder->NextSubPass();
                } else {
                    currentEncoder->EndPass();
                    RearBarriers(u, g);
                }
            },
            [&](const ComputePassTag &) {
                auto &compute = graph.computePasses[Index(u, graph)];
                FrontBarriers(u, g);
                RearBarriers(u, g);
            },
            [&](const CopyBlitTag &) {
                auto &cb = graph.copyBlitPasses[Index(u, graph)];
                FrontBarriers(u, g);
                RearBarriers(u, g);
            },
            [&](const PresentTag &) {
                auto &present = graph.presentPasses[Index(u, graph)];
                FrontBarriers(u, g);
                RearBarriers(u, g);
            },
            [&](const auto &) {}
        }, Tag(u, graph));
    }

}
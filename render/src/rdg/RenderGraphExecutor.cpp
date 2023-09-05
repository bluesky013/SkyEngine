//
// Created by Zach Lee on 2023/3/27.
//

#include <render/rdg/RenderGraphExecutor.h>
#include <rhi/Decode.h>

namespace sky::rdg {

    void RenderGraphExecutor::Barriers(const PmrHashMap<VertexType, std::vector<GraphBarrier>>& barrierSet) const
    {
        for (const auto &[first, second] : barrierSet) {
            const auto resID = first;
            const auto &barriers = second;
            std::visit(Overloaded{
                [&](const ImageTag &) {
                    auto &image = graph.resourceGraph.images[Index(resID, graph.resourceGraph)];
                    for (const auto &barrier : barriers) {
                        graph.context->mainCommandBuffer->QueueBarrier(image.desc.image,
                            rhi::ImageSubRange{static_cast<uint32_t>(barrier.range.base), static_cast<uint32_t>(barrier.range.range),
                                                                                          barrier.range.layer, barrier.range.layers,
                                                                                          rhi::GetAspectFlagsByFormat(image.desc.format)},
                            rhi::BarrierInfo{ barrier.srcFlags, barrier.dstFlags});
                    }
                },
                [&](const ImportImageTag &) {
                    auto &image = graph.resourceGraph.importImages[Index(resID, graph.resourceGraph)];
                    for (const auto &barrier : barriers) {
                        graph.context->mainCommandBuffer->QueueBarrier(image.desc.image,
                            rhi::ImageSubRange{static_cast<uint32_t>(barrier.range.base), static_cast<uint32_t>(barrier.range.range),
                                                                                          barrier.range.layer, barrier.range.layers,
                                                                                          rhi::GetAspectFlagsByFormat(image.desc.image->GetDescriptor().format)},
                            rhi::BarrierInfo{ barrier.srcFlags, barrier.dstFlags});
                    }

                },
                [&](const ImportSwapChainTag &) {
                    auto &image = graph.resourceGraph.swapChains[Index(resID, graph.resourceGraph)];
                    for (const auto &barrier : barriers) {
                        graph.context->mainCommandBuffer->QueueBarrier(image.desc.swapchain->GetImage(image.desc.imageIndex),
                            rhi::ImageSubRange{static_cast<uint32_t>(barrier.range.base), static_cast<uint32_t>(barrier.range.range),
                                               barrier.range.layer, barrier.range.layers,
                                               rhi::AspectFlagBit::COLOR_BIT},
                                               rhi::BarrierInfo{ barrier.srcFlags, barrier.dstFlags});
                    }
                },
                [&](const BufferTag &) {
                    auto &buffer = graph.resourceGraph.buffers[Index(resID, graph.resourceGraph)];
                    for (const auto &barrier : barriers) {
                        graph.context->mainCommandBuffer->QueueBarrier(buffer.desc.buffer, barrier.range.base, barrier.range.range,
                            rhi::BarrierInfo{ barrier.srcFlags, barrier.dstFlags});
                    }

                },
                [&](const ImportBufferTag &) {
                    auto &buffer = graph.resourceGraph.importBuffers[Index(resID, graph.resourceGraph)];
                    for (const auto &barrier : barriers) {
                        graph.context->mainCommandBuffer->QueueBarrier(buffer.desc.buffer, barrier.range.base, barrier.range.range,
                            rhi::BarrierInfo{ barrier.srcFlags, barrier.dstFlags});
                    }

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
            },
            [&](const CopyBlitTag &) {
                auto &cb = graph.copyBlitPasses[Index(u, graph)];
                Barriers(cb.frontBarriers);
            },
            [&](const PresentTag &) {
                auto &present = graph.presentPasses[Index(u, graph)];
                Barriers(present.frontBarriers);
            },
            [&](const RasterQueueTag &) {
                auto &queue = graph.rasterQueues[Index(u, graph)];

                for (auto &item : queue.drawItems) {
                    auto &tech = item.primitive->techniques[item.techIndex];

                    currentEncoder->BindPipeline(tech.pso);
                    currentEncoder->BindSet(0, item.primitive->passSet ? item.primitive->passSet : nullptr); // todo per pass set
                    currentEncoder->BindSet(1, item.primitive->batchSet ? item.primitive->batchSet : nullptr);
                    currentEncoder->BindSet(2, item.primitive->instanceSet ? item.primitive->instanceSet : nullptr);
                    currentEncoder->BindAssembly(item.primitive->va);
                    std::visit(Overloaded{
                        [&](const rhi::CmdDrawLinear &v) {
                            currentEncoder->DrawLinear(v);
                        },
                        [&](const rhi::CmdDrawIndexed &v) {
                            currentEncoder->DrawIndexed(v);
                        }
                    }, item.primitive->args);
                }
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

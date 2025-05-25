//
// Created by Zach Lee on 2023/3/27.
//

#include <render/rdg/RenderGraphExecutor.h>
#include <rhi/Decode.h>

namespace sky::rdg {

    rhi::ImagePtr GetGraphImage(ResourceGraph &resourceGraph, VertexType resID)
    {
        rhi::ImagePtr res;
        std::visit(Overloaded{
            [&](const ImageTag &) {
                res = resourceGraph.images[Index(resID, resourceGraph)].desc.image;
            },
            [&](const ImportImageTag &) {
                res = resourceGraph.importImages[Index(resID, resourceGraph)].desc.image;
            },
            [&](const ImportSwapChainTag &) {
                const auto &image = resourceGraph.swapChains[Index(resID, resourceGraph)];
                res = image.desc.swapchain->GetImage(image.desc.imageIndex);
            },
#ifdef SKY_ENABLE_XR
            [&](const ImportXRSwapChainTag &) {
                const auto &image = resourceGraph.xrSwapChains[Index(resID, resourceGraph)];
                res = image.desc.swapchain->GetImage(image.desc.imageIndex);
            },
#endif
            [&](const auto &) {
            }
        }, Tag(resID, resourceGraph));
        return res;
    }

    void RenderGraphExecutor::Barriers(const PmrHashMap<VertexType, std::vector<GraphBarrier>>& barrierSet) const
    {
        const auto &mainCommandBuffer = graph.context->MainCommandBuffer();
        for (const auto &[first, second] : barrierSet) {
            const auto resID = first;
            const auto &barriers = second;
            std::visit(Overloaded{
                [&](const ImageTag &) {
                    auto &image = graph.resourceGraph.images[Index(resID, graph.resourceGraph)];
                    for (const auto &barrier : barriers) {
                        mainCommandBuffer->QueueBarrier(image.desc.image,
                            rhi::ImageSubRange{static_cast<uint32_t>(barrier.range.base), static_cast<uint32_t>(barrier.range.range),
                                                                                          barrier.range.layer, barrier.range.layers,
                                                                                          rhi::GetAspectFlagsByFormat(image.desc.format)},
                            rhi::BarrierInfo{ barrier.srcFlags, barrier.dstFlags});
                    }
                },
                [&](const ImportImageTag &) {
                    auto &image = graph.resourceGraph.importImages[Index(resID, graph.resourceGraph)];
                    for (const auto &barrier : barriers) {
                        mainCommandBuffer->QueueBarrier(image.desc.image,
                            rhi::ImageSubRange{static_cast<uint32_t>(barrier.range.base), static_cast<uint32_t>(barrier.range.range),
                                                                                          barrier.range.layer, barrier.range.layers,
                                                                                          rhi::GetAspectFlagsByFormat(image.desc.image->GetDescriptor().format)},
                            rhi::BarrierInfo{ barrier.srcFlags, barrier.dstFlags});
                    }

                },
                [&](const ImportImageViewTag &) {
                    auto &image = graph.resourceGraph.importedViews[Index(resID, graph.resourceGraph)];
                    for (const auto &barrier : barriers) {
                        mainCommandBuffer->QueueBarrier(image.desc.image, image.desc.view->GetViewDesc().subRange,
                            rhi::BarrierInfo{ barrier.srcFlags, barrier.dstFlags });
                    }

                },
                [&](const ImportSwapChainTag &) {
                    auto &image = graph.resourceGraph.swapChains[Index(resID, graph.resourceGraph)];
                    for (const auto &barrier : barriers) {
                        mainCommandBuffer->QueueBarrier(image.desc.swapchain->GetImage(image.desc.imageIndex),
                            rhi::ImageSubRange{static_cast<uint32_t>(barrier.range.base), static_cast<uint32_t>(barrier.range.range),
                                               barrier.range.layer, barrier.range.layers,
                                               rhi::AspectFlagBit::COLOR_BIT},
                                               rhi::BarrierInfo{ barrier.srcFlags, barrier.dstFlags});
                    }
                },
#ifdef SKY_ENABLE_XR
                [&](const ImportXRSwapChainTag &) {
                    auto &image = graph.resourceGraph.xrSwapChains[Index(resID, graph.resourceGraph)];
                    for (const auto &barrier : barriers) {
                        mainCommandBuffer->QueueBarrier(image.desc.swapchain->GetImage(image.desc.imageIndex),
                                                        rhi::ImageSubRange{static_cast<uint32_t>(barrier.range.base), static_cast<uint32_t>(barrier.range.range),
                                                        barrier.range.layer, barrier.range.layers,
                                                        rhi::AspectFlagBit::COLOR_BIT},
                                                        rhi::BarrierInfo{ barrier.srcFlags, barrier.dstFlags});
                    }
                },
#endif
                [&](const BufferTag &) {
                    auto &buffer = graph.resourceGraph.buffers[Index(resID, graph.resourceGraph)];
                    for (const auto &barrier : barriers) {
                        mainCommandBuffer->QueueBarrier(buffer.desc.buffer, barrier.range.base, barrier.range.range,
                            rhi::BarrierInfo{ barrier.srcFlags, barrier.dstFlags});
                    }

                },
                [&](const ImportBufferTag &) {
                    auto &buffer = graph.resourceGraph.importBuffers[Index(resID, graph.resourceGraph)];
                    for (const auto &barrier : barriers) {
                        mainCommandBuffer->QueueBarrier(buffer.desc.buffer, barrier.range.base, barrier.range.range,
                            rhi::BarrierInfo{ barrier.srcFlags, barrier.dstFlags});
                    }

                },
                [&](const auto &) {}
            }, Tag(resID, graph.resourceGraph));
        }
        mainCommandBuffer->FlushBarriers();
    }

    [[maybe_unused]] void RenderGraphExecutor::discover_vertex(Vertex u, const Graph& g) // NOLINT
    {
        const auto &mainCommandBuffer = graph.context->MainCommandBuffer();
        std::visit(Overloaded{
            [&](const RasterPassTag &) {
                auto &raster = graph.rasterPasses[Index(u, graph)];
                Barriers(raster.frontBarriers);

                rhi::PassBeginInfo beginInfo = {};
                beginInfo.frameBuffer = raster.frameBuffer;
                beginInfo.renderPass  = raster.renderPass;
                beginInfo.clearCount  = static_cast<uint32_t>(raster.clearValues.size());
                beginInfo.clearValues = raster.clearValues.data();

                currentEncoder = mainCommandBuffer->EncodeGraphics();
                currentEncoder->BeginPass(beginInfo);

                // queue
                currentSubPassIndex = 0;
                currentSubPassNum = beginInfo.renderPass->GetSubPassNum();

                callStack.emplace_back(graph.names[u]);
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
                callStack.emplace_back(graph.names[u]);
            },
            [&](const TransitionTag &) {
                auto &transition = graph.transitionPasses[Index(u, graph)];
                Barriers(transition.frontBarriers);
                callStack.emplace_back(graph.names[u]);
            },
            [&](const CopyBlitTag &) {
                auto &cb = graph.copyBlitPasses[Index(u, graph)];
                Barriers(cb.frontBarriers);

                auto blit = mainCommandBuffer->EncodeBlit();
                rhi::BlitInfo info = {};
                info.srcRange = cb.srcRange;
                info.dstRange = cb.dstRange;
                info.srcOffsets[1].x = cb.srcExt.width;
                info.srcOffsets[1].y = cb.srcExt.height;
                info.srcOffsets[1].z = 1;

                info.dstOffsets[1].x = cb.dstExt.width;
                info.dstOffsets[1].y = cb.dstExt.height;
                info.dstOffsets[1].z = 1;
                blit->BlitTexture(GetGraphImage(graph.resourceGraph, cb.src), GetGraphImage(graph.resourceGraph, cb.dst), {info}, rhi::Filter::LINEAR);

                callStack.emplace_back(graph.names[u]);
            },
            [&](const PresentTag &) {
                auto &present = graph.presentPasses[Index(u, graph)];
                Barriers(present.frontBarriers);

#ifdef SKY_ENABLE_XR
                if (present.xrSwapChain) {
                    present.xrSwapChain->Present();
                }
#endif
                callStack.emplace_back(graph.names[u]);
            },
            [&](const RasterQueueTag &) {
                auto &queue = graph.rasterQueues[Index(u, graph)];
                for (auto &item : queue.drawItems) {
                    auto &batch = item.primitive->batches[item.techIndex];

                    currentEncoder->BindPipeline(batch.pso);
                    if (queue.resourceGroup != nullptr && ((batch.pso->GetDescriptorMask() & (1 << 0)) != 0u)) {
                        queue.resourceGroup->OnBind(*currentEncoder, 0);
                    } else {
                        graph.context->emptySet->OnBind(*currentEncoder, 0);
                    }

                    if (batch.batchGroup && ((batch.pso->GetDescriptorMask() & (1 << 1)) != 0u)) {
                        batch.batchGroup->OnBind(*currentEncoder, 1);
                    } else {
                        graph.context->emptySet->OnBind(*currentEncoder, 1);
                    }

                    if (item.primitive->instanceSet && ((batch.pso->GetDescriptorMask() & (1 << 2)) != 0u)) {
                        item.primitive->instanceSet->OnBind(*currentEncoder, 2);
                    }

                    if (batch.vao) {
                        currentEncoder->BindAssembly(batch.vao);
                    } else {
                        std::vector<rhi::BufferView> vertexBuffers;
                        item.primitive->geometry->FillVertexBuffer(vertexBuffers);
                        currentEncoder->BindVertexBuffers(vertexBuffers);
                    }

                    const auto &ib = item.primitive->geometry->indexBuffer;
                    if (ib.buffer) {
                        currentEncoder->BindIndexBuffer(ib.MakeView(), ib.indexType);
                    }

                    for (const auto &arg : item.primitive->args) {
                        std::visit(Overloaded{
                            [&](const rhi::CmdDrawLinear &v) {
                                currentEncoder->DrawLinear(v);
                                graph.context->rdgData.triangleData += v.vertexCount / 3 * v.instanceCount;
                            },
                            [&](const rhi::CmdDrawIndexed &v) {
                                currentEncoder->DrawIndexed(v);
                                graph.context->rdgData.triangleData += v.indexCount / 3 * v.instanceCount;
                            },
                            [&](const rhi::CmdDispatchMesh &v) {
                                currentEncoder->DispatchMesh(v);
                            },
                            [&](const rhi::Viewport &v) {
                                currentEncoder->SetViewport(1, &v);
                            },
                            [&](const rhi::Rect2D &v) {
                                currentEncoder->SetScissor(1, &v);
                            },
                            [&](const auto &) {}
                        }, arg);

                        graph.context->rdgData.drawCall++;
                    }
                }
            },
            [&](const FullScreenBlitTag &) {
                auto &fullScreen = graph.fullScreens[Index(u, graph)];
                currentEncoder->BindPipeline(fullScreen.pso);
                if (fullScreen.resourceGroup != nullptr) {
                    fullScreen.resourceGroup->OnBind(*currentEncoder, 0);
                }
                currentEncoder->DrawLinear({3, 1, 0, 0});
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
                callStack.pop_back();
            },
            [&](const ComputePassTag &) {
                auto &compute = graph.computePasses[Index(u, graph)];
                Barriers(compute.rearBarriers);
                callStack.pop_back();
            },
            [&](const CopyBlitTag &) {
                auto &cb = graph.copyBlitPasses[Index(u, graph)];
                Barriers(cb.rearBarriers);
                callStack.pop_back();
            },
            [&](const PresentTag &) {
                auto &present = graph.presentPasses[Index(u, graph)];
                Barriers(present.rearBarriers);
                callStack.pop_back();
            },
            [&](const TransitionTag &) {
                auto &transition = graph.transitionPasses[Index(u, graph)];
                Barriers(transition.rearBarriers);
                callStack.emplace_back(graph.names[u]);
            },
            [&](const auto &) {}
        }, Tag(u, graph));
    }

}

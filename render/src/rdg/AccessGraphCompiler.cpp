//
// Created by Zach Lee on 2023/5/31.
//

#include <render/rdg/AccessGraphCompiler.h>
#include <render/rdg/AccessUtils.h>
#include <rhi/Device.h>

namespace sky::rdg {
    namespace {
        template <typename V>
        void SetResourceVisited(V resAccessID, RenderGraph &graph) {
            auto &resAccess = graph.accessGraph.resources[Index(resAccessID, graph.accessGraph)];
            resAccess.visited = true;
        }

        void MergeBarrier(std::vector<GraphBarrier> &barriers, const AccessRes &res, const AccessCompiler::Graph ::edge_bundled &ep,
                          const rhi::AccessFlags& nextAccess,
                          RenderGraph &rdg)
        {
            GraphBarrier barrier = {};
            const auto sourceRange = GetAccessRange(rdg, res.resID);
            for (uint32_t i = 0; i < ep.range.range; ++i) {
                const auto mip = ep.range.base + i;
                for (uint32_t j = 0; j < ep.range.layers; ++j) {
                    const auto layer = ep.range.layer + j;
                    auto &back = barriers.emplace_back();
                    back.range.base = mip;
                    back.range.range = 1;
                    back.range.layer = layer;
                    back.range.layers = 1;
                    back.range.aspectMask = ep.range.aspectMask;
                    back.srcFlags = res.accesses[mip * sourceRange.layers + layer];
                    back.dstFlags |= nextAccess;
                }
            }
        }
    }

    void AccessCompiler::UpdateLifeTime(VertexType passID, VertexType resID)
    {
        // update lifeTime
        LifeTime *lifeTime = nullptr;
        std::visit(Overloaded{
            [&](const ImageTag &) {
                auto &src = rdg.resourceGraph.images[Index(resID, rdg.resourceGraph)];
                lifeTime = &src.lifeTime;
            },
            [&](const BufferTag &) {
                auto &src = rdg.resourceGraph.buffers[Index(resID, rdg.resourceGraph)];
                lifeTime = &src.lifeTime;
            },
            [&](const auto &) {}
        }, Tag(resID, rdg.resourceGraph));
        SKY_ASSERT(lifeTime != nullptr);

        auto parentPassID = rdg.subPasses[Index(passID, rdg)].parent;
        lifeTime->begin = std::min(lifeTime->begin, parentPassID);
        lifeTime->end = std::max(lifeTime->end, parentPassID);
    }

    void AccessCompiler::examine_edge(Edge u, const Graph& g)
    {
        const auto &srcTag = Tag(u.m_source, rdg.accessGraph);
        const auto &dstTag = Tag(u.m_target, rdg.accessGraph);

        VertexType passID = INVALID_VERTEX;
        VertexType resID = INVALID_VERTEX;
        auto &ep = rdg.accessGraph.graph[u];

        std::visit(Overloaded{
            [&](const AccessPassTag&, const AccessResTag&) {
                const auto &src = rdg.accessGraph.passes[Index(u.m_source, rdg.accessGraph)];
                const auto &dst = rdg.accessGraph.resources[Index(u.m_target, rdg.accessGraph)];
                passID = src.vertexID;
                resID = dst.resID;
                SetResourceVisited(u.m_target, rdg);
                UpdateLifeTime(passID, resID);

                // subPass dependencies
                boost::graph_traits<AccessGraph::Graph>::out_edge_iterator begin;
                boost::graph_traits<AccessGraph::Graph>::out_edge_iterator end;

                for (boost::tie(begin, end) = boost::out_edges(u.m_target, rdg.accessGraph.graph);
                     begin != end; ++begin) {
                    auto nextEdge = *begin;
                    const auto &nextEp = rdg.accessGraph.graph[nextEdge];
                    auto nextAccess = GetAccessFlags(nextEp.dependencyInfo); // nextAccess
                    auto nextPassAccessID = boost::target(nextEdge, rdg.accessGraph.graph);
                    auto nextPassID = rdg.accessGraph.passes[Index(nextPassAccessID, rdg.accessGraph)].vertexID;
                    UpdateLifeTime(nextPassID, resID);

                    const RasterSubPassTag *srcTag = std::get_if<RasterSubPassTag>(&Tag(passID, rdg));
                    const RasterSubPassTag *dstTag = std::get_if<RasterSubPassTag>(&Tag(nextPassID, rdg));

                    bool isSubPassDependency = false;
                    if (srcTag != nullptr && dstTag != nullptr) {
                        const auto &srcSubPass = rdg.subPasses[Index(passID, rdg)];
                        const auto &dstSubPass = rdg.subPasses[Index(nextPassID, rdg)];

                        // subPass dependency
                        if (srcSubPass.parent == dstSubPass.parent && srcSubPass.subPassID != dstSubPass.subPassID) {
                            auto &parent = rdg.rasterPasses[Index(srcSubPass.parent, rdg)];
                            auto &dep = parent.dependencies.emplace_back();
                            dep.src = srcSubPass.subPassID;
                            dep.dst = dstSubPass.subPassID;
                            dep.preAccess = GetAccessFlags(ep.dependencyInfo); // prevAccess
                            dep.nextAccess |= nextAccess;
                            isSubPassDependency = true;
                        }
                    }

                    // barrier
                    if (!isSubPassDependency) {
                        std::visit(Overloaded{
                            [&](const RasterSubPassTag &) {
                                const auto &srcSubPass = rdg.subPasses[Index(passID, rdg)];
                                auto &parent = rdg.rasterPasses[Index(srcSubPass.parent, rdg)];
                                auto &rearBarriers = parent.rearBarriers[resID];
                                const auto &tmp = nextEp;
                                MergeBarrier(rearBarriers, dst, tmp, nextAccess, rdg);
                            },
                            [&](const ComputePassTag &) {
                                auto &computePass = rdg.computePasses[Index(passID, rdg)];
                                auto &rearBarrier = computePass.rearBarriers[resID];
//                                MergeSubRange(rearBarrier.range, ep.range);
//                                rearBarrier.srcFlags = GetAccessFlags(ep.dependencyInfo);
//                                rearBarrier.dstFlags |= nextAccess;
                            },
                            [&](const CopyBlitTag &) {
                                auto &copyBlitPass = rdg.copyBlitPasses[Index(passID, rdg)];
                                auto &rearBarrier = copyBlitPass.rearBarriers[resID];
//                                MergeSubRange(rearBarrier.range, ep.range);
//                                rearBarrier.srcFlags = GetAccessFlags(ep.dependencyInfo);
//                                rearBarrier.dstFlags |= nextAccess;
                            },
                            [&](const auto &) {
                            }
                        }, Tag(passID, rdg));
                    }
                }
            },
            [&](const AccessResTag&, const AccessPassTag&) {
                // process external dependency
                const auto &src = rdg.accessGraph.resources[Index(u.m_source, rdg.accessGraph)];
                const auto &dst = rdg.accessGraph.passes[Index(u.m_target, rdg.accessGraph)];
                if (src.visited) {
                    return;
                }
                resID = src.resID;
                passID = dst.vertexID;

                std::visit(Overloaded{
                    [&](const RasterSubPassTag &) {
                        const auto &srcSubPass = rdg.subPasses[Index(passID, rdg)];
                        auto &parent = rdg.rasterPasses[Index(srcSubPass.parent, rdg)];
                        auto &frontBarrier = parent.frontBarriers[resID];
                        auto &back = frontBarrier.emplace_back();
                        back.range = ep.range;
                        back.srcFlags = rhi::AccessFlagBit::NONE;
                        back.dstFlags = GetAccessFlags(ep.dependencyInfo);
                    },
                    [&](const ComputePassTag &) {
                        auto &computePass = rdg.computePasses[Index(passID, rdg)];
                        auto &frontBarrier = computePass.frontBarriers[resID];
                        auto &back = frontBarrier.emplace_back();
                        back.range = ep.range;
                        back.srcFlags = rhi::AccessFlagBit::NONE;
                        back.dstFlags = GetAccessFlags(ep.dependencyInfo);
                    },
                    [&](const CopyBlitTag &) {
                        auto &copyBlitPass = rdg.copyBlitPasses[Index(passID, rdg)];
                        auto &frontBarrier = copyBlitPass.frontBarriers[resID];
                        auto &back = frontBarrier.emplace_back();
                        back.range = ep.range;
                        back.srcFlags = rhi::AccessFlagBit::NONE;
                        back.dstFlags = GetAccessFlags(ep.dependencyInfo);
                    },
                    [&](const auto &) {
                    }
                }, Tag(passID, rdg));
            },
            [](const auto&, const auto &) {
                SKY_ASSERT(false);
            }
        }, srcTag, dstTag);
    }

} // namespace sky::rdg

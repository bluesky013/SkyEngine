//
// Created by Zach Lee on 2023/5/31.
//

#include <render/rdg/AccessGraphCompiler.h>
#include <render/rdg/AccessUtils.h>
#include <rhi/Device.h>

namespace sky::rdg {
    namespace {
        void MergeBarrier(std::vector<GraphBarrier> &barriers, const AccessRes &prev, const AccessRes &next, RenderGraph &rdg)
        {
            barriers.emplace_back(GraphBarrier{prev.access, next.access, prev.subRange});


//            for (uint32_t i = 0; i < next.subRange.range; ++i) {
//                const auto mip = next.subRange.base + i;
//
//                rhi::AccessFlags lastPrevFlags = rhi::AccessFlagBit::NONE;
//                rhi::AccessFlags lastNextFlags = rhi::AccessFlagBit::NONE;
//
//                std::vector<GraphBarrier> rowBarriers;
//                for (uint32_t j = 0; j < next.subRange.layers; ++j) {
//                    const auto layer = next.subRange.layer + j;
//                    const auto &prevFlags = prev.accesses[mip * sourceRange.layers + layer];
//                    const auto &nextFlags = next.accesses[mip * sourceRange.layers + layer];
//
//                    if ((lastPrevFlags != prevFlags && lastNextFlags != nextFlags) || rowBarriers.empty()) {
//                        rowBarriers.emplace_back(GraphBarrier{prevFlags, nextFlags, {mip, 1, layer, 1}});
//                    } else {
//                        rowBarriers.back().range.layers++;
//                    }
//                    lastPrevFlags = prevFlags;
//                    lastNextFlags = nextFlags;
//                }
//
//                // try merge level barrier
//                if (!barriers.empty() && rowBarriers.size() == 1) {
//                    auto &back = barriers.back();
//                    if (back.range.layers == sourceRange.layers) {
//                        back.range.range++;
//                    }
//                } else {
//                    barriers.insert(barriers.end(), rowBarriers.begin(), rowBarriers.end());
//                }
//            }
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

        if (lifeTime != nullptr) {
            auto parentPassID = rdg.subPasses[Index(passID, rdg)].parent;
            lifeTime->begin = std::min(lifeTime->begin, parentPassID);
            lifeTime->end = std::max(lifeTime->end, passID);
        }
    }

    void AccessCompiler::examine_edge(Edge u, const Graph& g)
    {
        const auto &srcTag = Tag(u.m_source, rdg.accessGraph);
        const auto &dstTag = Tag(u.m_target, rdg.accessGraph);

        VertexType passID = INVALID_VERTEX;
        VertexType resID = INVALID_VERTEX;

        std::visit(Overloaded{
            [&](const AccessPassTag&, const AccessResTag&) {
                const auto &src = rdg.accessGraph.passes[Index(u.m_source, rdg.accessGraph)];
                const auto &dst = rdg.accessGraph.resources[Index(u.m_target, rdg.accessGraph)];
                passID = src.vertexID;
                resID = dst.resID;
                UpdateLifeTime(passID, resID);

                // subPass dependencies
                boost::graph_traits<AccessGraph::Graph>::out_edge_iterator begin;
                boost::graph_traits<AccessGraph::Graph>::out_edge_iterator end;

                for (boost::tie(begin, end) = boost::out_edges(u.m_target, rdg.accessGraph.graph);
                     begin != end; ++begin) {
                    auto nextEdge = *begin;
                    auto nextPassAccessID = boost::target(nextEdge, rdg.accessGraph.graph);
                    auto nextPassID = rdg.accessGraph.passes[Index(nextPassAccessID, rdg.accessGraph)].vertexID;
                    UpdateLifeTime(nextPassID, resID);

                    if (dst.nextAccessResID == INVALID_VERTEX) {
                        continue;
                    }
                    const auto &nextAccessRes = rdg.accessGraph.resources[Index(dst.nextAccessResID, rdg.accessGraph)];

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
//                            dep.src = srcSubPass.subPassID;
//                            dep.dst = dstSubPass.subPassID;
//                            dep.preAccess = ep.flags; // prevAccess
//                            dep.nextAccess |= nextAccess;
                            isSubPassDependency = true;
                        }
                    }

                    // barrier
                    if (!isSubPassDependency) {
                        std::visit(Overloaded{
                            [&](const RootTag &) {
                                const auto &srcSubPass = rdg.subPasses[Index(nextPassID, rdg)];
                                auto &parent = rdg.rasterPasses[Index(srcSubPass.parent, rdg)];
                                auto &frontBarriers = parent.frontBarriers[resID];  // use front barriers for external barrier.
                                MergeBarrier(frontBarriers, dst, nextAccessRes, rdg);
                            },
                            [&](const RasterSubPassTag &) {
                                const auto &srcSubPass = rdg.subPasses[Index(passID, rdg)];
                                auto &parent = rdg.rasterPasses[Index(srcSubPass.parent, rdg)];
                                auto &rearBarriers = parent.rearBarriers[resID];
                                MergeBarrier(rearBarriers, dst, nextAccessRes, rdg);
                            },
                            [&](const ComputePassTag &) {
                                auto &computePass = rdg.computePasses[Index(passID, rdg)];
                                auto &rearBarriers = computePass.rearBarriers[resID];
                                MergeBarrier(rearBarriers, dst, nextAccessRes, rdg);
                            },
                            [&](const CopyBlitTag &) {
                                auto &copyBlitPass = rdg.copyBlitPasses[Index(passID, rdg)];
                                auto &rearBarriers = copyBlitPass.rearBarriers[resID];
                                MergeBarrier(rearBarriers, dst, nextAccessRes, rdg);
                            },
                            [&](const TransitionTag &) {
                                auto &transition = rdg.transitionPasses[Index(passID, rdg)];
                                auto &rearBarriers = transition.rearBarriers[resID];
                                MergeBarrier(rearBarriers, dst, nextAccessRes, rdg);
                            },
                            [&](const auto &) {
                            }
                        }, Tag(passID, rdg));
                    }
                }
            },
            [](const auto&, const auto &) {
            }
        }, srcTag, dstTag);
    }

} // namespace sky::rdg

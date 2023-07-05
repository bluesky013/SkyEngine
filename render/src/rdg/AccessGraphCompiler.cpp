//
// Created by Zach Lee on 2023/5/31.
//

#include <render/rdg/AccessGraphCompiler.h>

namespace std {

    template <>
    struct hash<sky::rdg::AttachmentType> {
        size_t operator()(const sky::rdg::AttachmentType &type) const noexcept
        {
            return std::visit(sky::Overloaded{[](const auto &arg) {
                return static_cast<uint32_t>(arg);
            }}, type);
        }
    };

    template <>
    struct equal_to<sky::rdg::AttachmentType> {
        bool operator()(const sky::rdg::AttachmentType &lhs, const sky::rdg::AttachmentType &rhs) const noexcept
        {
            return lhs == rhs;
        }
    };

} // namespace std

namespace sky::rdg {

    namespace {
        /**
         *  |            AttachmentType                 |      RW      |   VISIBILITY   |
         *  | RT RS IN DS SR | CBV SRV UAV | CPS  CPD   |   R    W     |   VS  FS  CS   |
         *  | X                                             X    X             X        | COLOR_READ | COLOR_WRITE
         *  |    X                                          X    X             X        | COLOR_READ | COLOR_WRITE
         *  |       X                                       X                  X        | COLOR_INPUT | DEPTH_STENCIL_INPUT
         *  |          X                                    X    X             X        | DEPTH_STENCIL_READ | DEPTH_STENCIL_WRITE
         *  | X     X                                       X    X             X        | COLOR_INOUT_READ | COLOR_INOUT_WRITE
         *  |       X  X                                    X    X             X        | DEPTH_STENCIL_INOUT_READ | DEPTH_STENCIL_INOUT_WRITE
         *  |             X                                 X                  X        | SHADING_RATE
         *  ----------------------------------------------------------------------------
         *  |                  X                            X              X   X   X    | XX_CBV
         *  |                      X                        X              X   X   X    | XX_SRV
         *  |                          X                    X    X         X   X   X    | XX_READ_UAV | XX_WRITE_UAV
         *  ----------------------------------------------------------------------------
         *  |                                X                                          | TRANSFER_READ
         *  |                                     X                                     | TRANSFER_WRITE
         */

        struct AccessFlagMask {
            uint32_t writeMask;
            uint32_t visibilityMask;
            rhi::AccessFlag readAccess;
            rhi::AccessFlag writeAccess;
        };

        const std::unordered_map<AttachmentType, AccessFlagMask> AttachmentMask {
            {RasterTypeBit::COLOR,                                {0x3, 0x2, rhi::AccessFlag::COLOR_READ,               rhi::AccessFlag::COLOR_WRITE}},
            {RasterTypeBit::RESOLVE,                              {0x3, 0x2, rhi::AccessFlag::COLOR_READ,               rhi::AccessFlag::COLOR_WRITE}},
            {RasterTypeBit::INPUT,                                {0x1, 0x2, rhi::AccessFlag::COLOR_READ,               rhi::AccessFlag::COLOR_WRITE}},
            {RasterTypeBit::DEPTH_STENCIL,                        {0x3, 0x2, rhi::AccessFlag::DEPTH_STENCIL_READ,       rhi::AccessFlag::DEPTH_STENCIL_WRITE}},
            {RasterTypeBit::COLOR | RasterTypeBit::INPUT,         {0x3, 0x2, rhi::AccessFlag::COLOR_INOUT_READ,         rhi::AccessFlag::COLOR_INOUT_WRITE}},
            {RasterTypeBit::DEPTH_STENCIL | RasterTypeBit::INPUT, {0x3, 0x2, rhi::AccessFlag::DEPTH_STENCIL_INOUT_READ, rhi::AccessFlag::DEPTH_STENCIL_INOUT_WRITE}},
            {RasterTypeBit::SHADING_RATE,                         {0x1, 0x2, rhi::AccessFlag::SHADING_RATE,             rhi::AccessFlag::NONE}},
            {ComputeType::CBV,                                    {0x1, 0xFF}},
            {ComputeType::SRV,                                    {0x1, 0xFF}},
            {ComputeType::UAV,                                    {0x3, 0xFF}},
            {TransferType::SRC,                                   {0x3, 0xFF}},
            {TransferType::DST,                                   {0x3, 0xFF}},
        };

        const std::unordered_map<rhi::ShaderStageFlagBit, rhi::AccessFlag> CBVMap {
            {rhi::ShaderStageFlagBit::VS, rhi::AccessFlag::VERTEX_CBV},
            {rhi::ShaderStageFlagBit::FS, rhi::AccessFlag::FRAGMENT_CBV},
            {rhi::ShaderStageFlagBit::CS, rhi::AccessFlag::COMPUTE_CBV},
        };

        std::vector<rhi::AccessFlag> GetAccessFlag(const AccessEdge &edge) {
            SKY_ASSERT(AttachmentMask.count(edge.type));
            auto accessMask = AttachmentMask.at(edge.type);
            SKY_ASSERT((edge.access & ResourceAccess(accessMask.writeMask)) == edge.access);
            SKY_ASSERT((edge.visibility & rhi::ShaderStageFlags(accessMask.visibilityMask)) == edge.visibility);

            std::vector<rhi::AccessFlag> res;
            std::visit(Overloaded{
                           [&](const RasterType &type) {
                               if (edge.access & ResourceAccessBit::READ) { res.emplace_back(accessMask.readAccess); }
                               if (edge.access & ResourceAccessBit::WRITE) { res.emplace_back(accessMask.writeAccess); }
                           },
                           [&](const ComputeType &type) {
                               auto fn = [&](rhi::ShaderStageFlagBit stage, ComputeType type) {
                                   auto base = static_cast<uint32_t>(CBVMap.at(stage)) + static_cast<uint32_t>(type);
                                   if (edge.access & ResourceAccessBit::READ) {
                                       res.emplace_back(static_cast<rhi::AccessFlag>(base));
                                   }
                                   if (edge.access & ResourceAccessBit::WRITE) {
                                       res.emplace_back(static_cast<rhi::AccessFlag>(base + 1));
                                   }
                               };

                               if (edge.visibility & rhi::ShaderStageFlagBit::VS) {
                                   fn(rhi::ShaderStageFlagBit::VS, type);
                               }
                               if (edge.visibility & rhi::ShaderStageFlagBit::FS) {
                                   fn(rhi::ShaderStageFlagBit::FS, type);
                               }
                               if (edge.visibility & rhi::ShaderStageFlagBit::CS) {
                                   fn(rhi::ShaderStageFlagBit::CS, type);
                               }
                           },
                           [&](const TransferType &type) {
                               res.emplace_back(type == TransferType::SRC ? rhi::AccessFlag::TRANSFER_READ : rhi::AccessFlag::TRANSFER_WRITE);
                           }},
                       edge.type);
            return res;
        }
    }

    void AccessCompiler::UpdateLifeTime(VertexType passID, VertexType resID)
    {
        auto sourceId = rdg.resourceGraph.sources[resID];
        // update life time
        LifeTime *lifeTime = nullptr;
        std::visit(Overloaded{
            [&](const ImageTag &) {
                auto &src = rdg.resourceGraph.images[Index(sourceId, rdg.resourceGraph)];
                lifeTime = &src.lifeTime;
            },
            [&](const BufferTag &) {
                auto &src = rdg.resourceGraph.buffers[Index(sourceId, rdg.resourceGraph)];
                lifeTime = &src.lifeTime;
            },
            [&](const auto &) {}
        }, Tag(sourceId, rdg.resourceGraph));
        SKY_ASSERT(lifeTime != nullptr);

        auto parentPassID = rdg.subPasses[Index(passID, rdg)].parent;
        if (lifeTime->begin == INVALID_VERTEX) {
            lifeTime->begin = parentPassID;
        }
        lifeTime->end = parentPassID;
    }

    void AccessCompiler::examine_edge(Edge u, const Graph& g)
    {
        auto &srcTag = Tag(u.m_source, rdg.accessGraph);
        auto &dstTag = Tag(u.m_target, rdg.accessGraph);

        VertexType passID = INVALID_VERTEX;
        VertexType resID = INVALID_VERTEX;
        auto &ep = rdg.accessGraph.graph[u];

        std::visit(Overloaded{
            [&](const AccessPassTag&, const AccessResTag&) {
                const auto &src = rdg.accessGraph.passes[Index(u.m_source, rdg.accessGraph)];
                const auto &dst = rdg.accessGraph.resources[Index(u.m_target, rdg.accessGraph)];
                passID = src.vertexID;
                resID = dst.resID;
                UpdateLifeTime(passID, resID);

                // subPass dependencies
                boost::graph_traits<AccessGraph::Graph>::out_edge_iterator begin, end;
                for (boost::tie(begin, end) = boost::out_edges(u.m_target, rdg.accessGraph.graph);
                     begin != end; ++begin) {
                    auto nextEdge = *begin;
                    auto nextAccess = GetAccessFlag(rdg.accessGraph.graph[nextEdge]); // nextAccess
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
                        if (srcSubPass.parent == dstSubPass.parent) {
                            auto &parent = rdg.rasterPasses[Index(srcSubPass.parent, rdg)];
                            auto &dep = parent.dependencies.emplace_back();
                            dep.src = srcSubPass.subPassID;
                            dep.dst = dstSubPass.subPassID;
                            dep.preAccess = GetAccessFlag(ep); // prevAccess
                            dep.nextAccess.insert(dep.nextAccess.end(), nextAccess.begin(), nextAccess.end());
                            isSubPassDependency = true;
                        }
                    }

                    // barrier
                    if (!isSubPassDependency) {
                        std::visit(Overloaded{
                            [&](const RasterSubPassTag &) {
                                const auto &srcSubPass = rdg.subPasses[Index(passID, rdg)];
                                auto &parent = rdg.rasterPasses[Index(srcSubPass.parent, rdg)];
                                auto &srcFlags = parent.rearBarriers[resID].srcFlags;
                                auto &dstFlags = parent.rearBarriers[resID].dstFlags;
                                srcFlags = GetAccessFlag(ep);
                                dstFlags.insert(dstFlags.end(), nextAccess.begin(), nextAccess.end());
                            },
                            [&](const ComputePassTag &) {
                                auto &computePass = rdg.computePasses[Index(passID, rdg)];
                                auto &srcFlags = computePass.rearBarriers[resID].srcFlags;
                                auto &dstFlags = computePass.rearBarriers[resID].dstFlags;
                                srcFlags = GetAccessFlag(ep);
                                dstFlags.insert(dstFlags.end(), nextAccess.begin(), nextAccess.end());
                            },
                            [&](const CopyBlitTag &) {
                                auto &copyBlitPass = rdg.copyBlitPasses[Index(passID, rdg)];
                                auto &srcFlags = copyBlitPass.rearBarriers[resID].srcFlags;
                                auto &dstFlags = copyBlitPass.rearBarriers[resID].dstFlags;
                                srcFlags = GetAccessFlag(ep);
                                dstFlags.insert(dstFlags.end(), nextAccess.begin(), nextAccess.end());
                            },
                            [&](const auto &) {
                            }
                        }, Tag(passID, rdg));
                    }
                }
            },
            [&](const AccessResTag&, const AccessPassTag&) {
                // process external dependency
            },
            [](const auto&, const auto &) {
                SKY_ASSERT(false);
            }
        }, srcTag, dstTag);
    }

} // namespace sky::rdg

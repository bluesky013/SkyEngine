//
// Created by Zach Lee on 2023/3/27.
//

#include <core/logger/Logger.h>
#include <render/rdg/RenderGraphVisitors.h>

static const char *TAG = "RDG";

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
            {RasterTypeBit::COLOR,         {0x3, 0x2, rhi::AccessFlag::COLOR_READ,               rhi::AccessFlag::COLOR_WRITE}},
            {RasterTypeBit::RESOLVE,       {0x3, 0x2, rhi::AccessFlag::COLOR_READ,               rhi::AccessFlag::COLOR_WRITE}},
            {RasterTypeBit::INPUT,         {0x1, 0x2, rhi::AccessFlag::COLOR_READ,               rhi::AccessFlag::COLOR_WRITE}},
            {RasterTypeBit::DEPTH_STENCIL, {0x3, 0x2, rhi::AccessFlag::DEPTH_STENCIL_READ,       rhi::AccessFlag::DEPTH_STENCIL_WRITE}},
            {RasterTypeBit::COLOR |
             RasterTypeBit::INPUT,         {0x3, 0x2, rhi::AccessFlag::COLOR_INOUT_READ,         rhi::AccessFlag::COLOR_INOUT_WRITE}},
            {RasterTypeBit::DEPTH_STENCIL |
             RasterTypeBit::INPUT,         {0x3, 0x2, rhi::AccessFlag::DEPTH_STENCIL_INOUT_READ, rhi::AccessFlag::DEPTH_STENCIL_INOUT_WRITE}},
            {RasterTypeBit::SHADING_RATE,  {0x1, 0x2, rhi::AccessFlag::SHADING_RATE,             rhi::AccessFlag::NONE}},
            {ComputeType::CBV,             {0x1, 0xFF}},
            {ComputeType::SRV,             {0x1, 0xFF}},
            {ComputeType::UAV,             {0x3, 0xFF}},
            {TransferType::SRC,            {0x3, 0xFF}},
            {TransferType::DST,            {0x3, 0xFF}},
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

    void AccessCompiler::examine_edge(Edge u, const Graph& g) {
        auto &srcTag = Tag(u.m_source, rdg.accessGraph);
        auto &dstTag = Tag(u.m_target, rdg.accessGraph);

        VertexType passID = INVALID_VERTEX;
        VertexType resID = INVALID_VERTEX;
        auto &ep = rdg.accessGraph.graph[u];

        std::visit(Overloaded{
            [&](const AccessPassTag&, const AccessResTag&) {
                auto &src = rdg.accessGraph.passes[Index(u.m_source, rdg.accessGraph)];
                auto &dst = rdg.accessGraph.resources[Index(u.m_target, rdg.accessGraph)];
                dst.prevAccess = GetAccessFlag(ep);`
                passID = src.vertexID;
                resID = dst.resID;
            },
            [&](const AccessResTag&, const AccessPassTag&) {
                auto &src = rdg.accessGraph.resources[Index(u.m_source, rdg.accessGraph)];
                auto &dst = rdg.accessGraph.passes[Index(u.m_target, rdg.accessGraph)];
                src.nextAccess = GetAccessFlag(ep);
                resID = src.resID;
                passID = dst.vertexID;
            },
            [](const auto&, const auto &) {
                SKY_ASSERT(false);
            }
        }, srcTag, dstTag);

        auto sourceId = rdg.resourceGraph.sources[resID];

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

        if (lifeTime->begin == INVALID_VERTEX) {
            lifeTime->begin = passID;
        }
        lifeTime->end = passID;
    }

    void ResourceGraphCompiler::tree_edge(Edge u, const Graph &g) {
        std::variant<std::monostate, rhi::ImageViewPtr, rhi::BufferViewPtr> source;
        std::visit(Overloaded{
            [&](const ImageTag &) {
                auto &src = graph.images[Index(u.m_source, graph)];
                source = src.res;
            },
            [&](const BufferTag &) {
                auto &src = graph.buffers[Index(u.m_source, graph)];
                source = src.res;
            },
            [&](const ImageViewTag &) {
                auto &src = graph.imageViews[Index(u.m_source, graph)];
                source = src.res;
            },
            [&](const BufferViewTag &) {
                auto &src = graph.bufferViews[Index(u.m_source, graph)];
                source = src.res;
            },
            [&](const ImportImageTag &) {
                auto &src = graph.importImages[Index(u.m_source, graph)];
                source = src.res;
            },
            [&](const ImportBufferTag &) {
                auto &src = graph.importBuffers[Index(u.m_source, graph)];
                source = src.res;
            },
            [&](const auto &) {}
        }, Tag(u.m_source, graph));

        std::visit(Overloaded{
            [&](const rhi::ImageViewPtr &view) {
                auto &dst = graph.imageViews[Index(u.m_target, graph)];
                dst.res = view->CreateView(dst.desc.view);
            },
            [&](const rhi::BufferViewPtr &view) {
                auto &dst = graph.bufferViews[Index(u.m_target, graph)];
                dst.res = view->CreateView(dst.desc.view);
            },
            [&](const auto &) {}
        }, source);
    }

    void ResourceGraphCompiler::discover_vertex(Vertex u, const Graph& g) {
        std::visit(Overloaded{
            [&](const ImageTag &) {
                auto &image = graph.images[Index(u, graph)];
                image.res = graph.context->pool->requestImage(image.desc);
                LOG_I(TAG, "compile resource %s, lifeTime[%u, %u]...", Name(u, graph).c_str(),
                      image.lifeTime.begin, image.lifeTime.end);
            },
            [&](const BufferTag &) {
                auto &buffer = graph.buffers[Index(u, graph)];
                buffer.res = graph.context->pool->requestBuffer(buffer.desc);
                LOG_I(TAG, "compile resource %s, lifeTime[%u, %u]...", Name(u, graph).c_str(),
                      buffer.lifeTime.begin, buffer.lifeTime.end);
            },
            [&](const ImportImageTag &) {
                auto &image = graph.importImages[Index(u, graph)];
                const auto &info = image.desc.image->GetDescriptor();

                rhi::ImageViewDesc viewDesc = {};
                viewDesc.subRange = {0, info.mipLevels, 0, info.arrayLayers};
                viewDesc.mask = image.desc.mask;
                viewDesc.viewType = image.desc.viewType;
                image.res = image.desc.image->CreateView(viewDesc);
            },
            [&](const ImportBufferTag &) {
                auto &buffer = graph.importBuffers[Index(u, graph)];
                const auto &info = buffer.desc.buffer->GetBufferDesc();

                rhi::BufferViewDesc viewDesc = {};
                viewDesc.offset = 0;
                viewDesc.range = info.size;
                buffer.res = buffer.desc.buffer->CreateView(viewDesc);
            },
            [&](const auto &) {}
        }, Tag(u, graph));
    }

    void RenderGraphPassCompiler::tree_edge(Edge e, const Graph &g) {
        std::visit(Overloaded{
            [&](const RasterPassTag &) {
                auto &raster = graph.rasterPasses[Index(e.m_source, graph)];
                raster.subPasses.emplace_back(static_cast<VertexType>(e.m_target));
            },
            [&](const auto &) {}
        }, Tag(e.m_source, graph));
    }

    void RenderGraphPassCompiler::discover_vertex(Vertex u, const Graph& g) {
        LOG_I(TAG, "compile passes %s....", Name(u, graph).c_str());
        std::visit(Overloaded{
            [&](const ComputePassTag &) {
                auto &compute = graph.computePasses[Index(u, graph)];
            },
            [&](const CopyBlitTag &) {
                auto &cb = graph.copyBlitPasses[Index(u, graph)];
            },
            [&](const PresentTag &) {
                auto &present = graph.presentPasses[Index(u, graph)];
            },
            [&](const auto &) {}
        }, Tag(u, graph));
    }
}

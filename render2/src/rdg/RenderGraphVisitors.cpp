//
// Created by Zach Lee on 2023/3/27.
//

#include <core/logger/Logger.h>
#include <render/rdg/RenderGraphVisitors.h>

static const char *TAG = "RDG";

namespace sky::rdg {

    // NONE
    // INDIRECT_BUFFER
    // INDEX_BUFFER
    // VERTEX_BUFFER

    // VERTEX_CBV
    // VERTEX_SRV
    // VERTEX_READ_UAV
    // VERTEX_WRITE_UAV

    // FRAGMENT_CBV
    // FRAGMENT_SRV
    // FRAGMENT_READ_UAV
    // FRAGMENT_WRITE_UAV

    // COMPUTE_CBV
    // COMPUTE_SRV
    // COMPUTE_READ_UAV
    // COMPUTE_WRITE_UAV

    // SHADING_RATE
    // COLOR_INPUT
    // DEPTH_STENCIL_INPUT
    // COLOR_READ
    // COLOR_WRITE
    // DEPTH_STENCIL_READ
    // DEPTH_STENCIL_WRITE

    // TRANSFER_READ
    // TRANSFER_WRITE
    // PRESENT
    // GENERAL

    namespace {
        /**
         *  |            AttachmentType                 |      RW      |   VISIBILITY   |
         *  | RT RS IN DS SR | CBV SRV UAV | CPS  CPD   |   R    W     |   VS  FS  CS   |
         *  | X                                             X    X             X        | COLOR_READ | COLOR_WRITE
         *  |    X                                          X    X             X        | COLOR_READ | COLOR_WRITE
         *  |       X                                       X                  X        | COLOR_INPUT | DEPTH_STENCIL_INPUT
         *  |          X                                    X    X             X        | DEPTH_STENCIL_READ | DEPTH_STENCIL_WRITE
         *  |             X                                 X                  X        | SHADING_RATE
         *  ----------------------------------------------------------------------------
         *  |                  X                            X              X   X   X    | XX_CBV
         *  |                      X                        X              X   X   X    | XX_SRV
         *  |                          X                    X    X         X   X   X    | XX_READ_UAV | XX_WRITE_UAV
         *  ----------------------------------------------------------------------------
         *  |                                X              X                           | TRANSFER_READ
         *  |                                     X              X                      | TRANSFER_WRITE
         */


        rhi::AccessFlag GetAccessFlag(const AccessEdge &edge) {

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
                passID = src.vertexID;
                resID = dst.resID;
            },
            [&](const AccessResTag&, const AccessPassTag&) {
                auto &src = rdg.accessGraph.resources[Index(u.m_source, rdg.accessGraph)];
                auto &dst = rdg.accessGraph.passes[Index(u.m_target, rdg.accessGraph)];
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
                lifeTime = &src.desc.lifeTime;
            },
            [&](const BufferTag &) {
                auto &src = rdg.resourceGraph.buffers[Index(sourceId, rdg.resourceGraph)];
                lifeTime = &src.desc.lifeTime;
            },
            [&](const auto &) {}
        }, Tag(sourceId, rdg.resourceGraph));
        SKY_ASSERT(lifeTime != nullptr);

        if (lifeTime->begin == INVALID_VERTEX) {
            lifeTime->begin = passID;
        } else {
            lifeTime->end = passID;
        }
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
                      image.desc.lifeTime.begin, image.desc.lifeTime.end);
            },
            [&](const BufferTag &) {
                auto &buffer = graph.buffers[Index(u, graph)];
                buffer.res = graph.context->pool->requestBuffer(buffer.desc);
                LOG_I(TAG, "compile resource %s, lifeTime[%u, %u]...", Name(u, graph).c_str(),
                      buffer.desc.lifeTime.begin, buffer.desc.lifeTime.end);
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

    void RenderGraphPassCompiler::Compile(RasterPass &pass)
    {
    }

    void RenderGraphPassCompiler::Compile(ComputePass &pass)
    {
    }

    void RenderGraphPassCompiler::Compile(CopyBlitPass &pass)
    {
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

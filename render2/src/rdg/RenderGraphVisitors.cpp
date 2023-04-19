//
// Created by Zach Lee on 2023/3/27.
//

#include <core/logger/Logger.h>
#include <render/rdg/RenderGraphVisitors.h>

static const char *TAG = "RDG";

namespace sky::rdg {

    namespace {
        void MergeLifeTime(LifeTime &parent, const LifeTime &child)
        {
            if (parent.begin == INVALID_VERTEX) {
                parent.begin = child.begin;
                parent.end = child.end;
            } else {
                parent.begin = std::min(parent.begin, child.begin);
                parent.end = std::max(parent.end, child.end);
            }
        }
    }

    void LifeTimeVisitor::tree_edge(Edge u, const Graph &g)
    {
        // merge lifeTime to parent
        auto &parentLife = Life(u.m_source, graph);
        auto &childLife = Life(u.m_target, graph);
        MergeLifeTime(parentLife, childLife);
    }

    void ResourceGraphCompiler::tree_edge(Edge u, const Graph &g)
    {
        std::variant<std::monostate, rhi::ImageViewPtr, rhi::BufferViewPtr> source;
        std::visit(Overloaded{[&](const ImageTag &) {
                                  auto &src = graph.images[Index(u.m_source, graph)];
                                  source    = src.res;
                              },
                              [&](const BufferTag &) {
                                  auto &src = graph.buffers[Index(u.m_source, graph)];
                                  source    = src.res;
                              },
                              [&](const ImageViewTag &) {
                                  auto &src = graph.imageViews[Index(u.m_source, graph)];
                                  source    = src.res;
                              },
                              [&](const BufferViewTag &) {
                                  auto &src = graph.bufferViews[Index(u.m_source, graph)];
                                  source    = src.res;
                              },
                              [&](const ImportImageTag &) {
                                  auto &src = graph.importImages[Index(u.m_source, graph)];
                                  source    = src.res;
                              },
                              [&](const ImportBufferTag &) {
                                  auto &src = graph.importBuffers[Index(u.m_source, graph)];
                                  source    = src.res;
                              },
                              [&](const auto &) {}
                   }, Tag(u.m_source, graph));

        std::visit(Overloaded{[&](const rhi::ImageViewPtr &view) {
                                  auto &dst = graph.imageViews[Index(u.m_target, graph)];
                                  dst.res   = view->CreateView(dst.desc.view);
                              },
                              [&](const rhi::BufferViewPtr &view) {
                                  auto &dst = graph.bufferViews[Index(u.m_target, graph)];
                                  dst.res   = view->CreateView(dst.desc.view);
                              },
                              [&](const auto &) {}
                   }, source);
    }

    void ResourceGraphCompiler::discover_vertex(Vertex u, const Graph& g)
    {
        auto &life = Life(u, graph);
        LOG_I(TAG, "compile resource %s, lifeTime[%u, %u]...", Name(u, graph).c_str(), life.begin, life.end);
        std::visit(Overloaded{
                       [&](const ImageTag &) {
                           auto &image = graph.images[Index(u, graph)];
                           image.res = graph.context->pool->requestImage(image.desc);
                       },
                       [&](const BufferTag &) {
                           auto &buffer = graph.buffers[Index(u, graph)];
                           buffer.res = graph.context->pool->requestBuffer(buffer.desc);
                       },
                       [&](const ImportImageTag &) {
                           auto &image = graph.importImages[Index(u, graph)];
                           auto &info = image.desc.image->GetDescriptor();

                           rhi::ImageViewDesc viewDesc = {};
                           viewDesc.subRange = {0, info.mipLevels, 0, info.arrayLayers};
                           viewDesc.mask = image.desc.mask;
                           viewDesc.viewType = image.desc.viewType;
                           image.res = image.desc.image->CreateView(viewDesc);
                       },
                       [&](const ImportBufferTag &) {
                           auto &buffer = graph.importBuffers[Index(u, graph)];
                           auto &info = buffer.desc.buffer->GetBufferDesc();

                           rhi::BufferViewDesc viewDesc = {};
                           viewDesc.offset = 0;
                           viewDesc.range  = info.size;
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

    void RenderGraphPassCompiler::tree_edge(Edge e, const Graph &g)
    {
        std::visit(Overloaded{
                       [&](const RasterPassTag &) {
                           auto &raster = graph.rasterPasses[Index(e.m_source, graph)];
                           raster.subPasses.emplace_back(static_cast<VertexType>(e.m_target));
                       },
                       [&](const auto &) {}
                   }, Tag(e.m_source, graph));
    }

    void RenderGraphPassCompiler::discover_vertex(Vertex u, const Graph& g)
    {
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

    void RenderDependencyCompiler::examine_edge(Edge u, const Graph& g)
    {
        LOG_I(TAG, "dependency [%s] -> [%s]", Name(u.m_source, graph).c_str(), Name(u.m_target, graph).c_str());
    }
}

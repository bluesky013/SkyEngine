//
// Created by Zach Lee on 2023/3/27.
//

#include <render/rdg/RenderGraphCompiler.h>
#include <core/logger/Logger.h>

static const char *TAG = "RDG";

namespace sky::rdg {

    void RenderResourceCompiler::tree_edge(Edge u, const Graph& g)
    {
        std::variant<std::monostate, rhi::ImageViewPtr, rhi::BufferViewPtr> source;
        std::visit(Overloaded{[&](const ImageTag &) {
                                  auto &src = graph.images[index(u.m_source, graph)];
                                  source    = src.res;
                              },
                              [&](const BufferTag &) {
                                  auto &src = graph.buffers[index(u.m_source, graph)];
                                  source    = src.res;
                              },
                              [&](const ImageViewTag &) {
                                  auto &src = graph.imageViews[index(u.m_source, graph)];
                                  source    = src.res;
                              },
                              [&](const BufferViewTag &) {
                                  auto &src = graph.bufferViews[index(u.m_source, graph)];
                                  source    = src.res;
                              },
                              [&](const ImportImageTag &) {
                                  auto &src = graph.importImages[index(u.m_source, graph)];
                                  source    = src.res;
                              },
                              [&](const ImportBufferTag &) {
                                  auto &src = graph.importBuffers[index(u.m_source, graph)];
                                  source    = src.res;
                              },
                              [&](const auto &) {}
                   }, tag(u.m_source, graph));

        std::visit(Overloaded{[&](const rhi::ImageViewPtr &view) {
                                  auto &dst = graph.imageViews[index(u.m_target, graph)];
                                  dst.res   = view->CreateView(dst.desc.view);
                              },
                              [&](const rhi::BufferViewPtr &view) {
                                  auto &dst = graph.bufferViews[index(u.m_target, graph)];
                                  dst.res   = view->CreateView(dst.desc.view);
                              },
                              [&](const auto &) {}
                   }, source);
    }

    void RenderResourceCompiler::discover_vertex(Vertex u, const Graph& g)
    {
        LOG_I(TAG, "compile resource %s....", name(u, graph).c_str());
        std::visit(Overloaded{
                       [&](const ImageTag &) {
                           auto &image = graph.images[index(u, graph)];
                           image.res = graph.context->pool->requestImage(image.desc);
                       },
                       [&](const BufferTag &) {
                           auto &buffer = graph.buffers[index(u, graph)];
                           buffer.res = graph.context->pool->requestBuffer(buffer.desc);
                       },
                       [&](const ImportImageTag &) {
                           auto &image = graph.importImages[index(u, graph)];
                           auto &info = image.desc.image->GetDescriptor();

                           rhi::ImageViewDesc viewDesc = {};
                           viewDesc.subRange = {0, info.mipLevels, 0, info.arrayLayers};
                           viewDesc.mask = image.desc.mask;
                           viewDesc.viewType = image.desc.viewType;
                           image.res = image.desc.image->CreateView(viewDesc);
                       },
                       [&](const ImportBufferTag &) {
                           auto &buffer = graph.importBuffers[index(u, graph)];
                           auto &info = buffer.desc.buffer->GetBufferDesc();

                           rhi::BufferViewDesc viewDesc = {};
                           viewDesc.offset = 0;
                           viewDesc.range  = info.size;
                           buffer.res = buffer.desc.buffer->CreateView(viewDesc);
                       },
                       [&](const auto &) {}
        }, tag(u, graph));
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
                           auto &raster = graph.rasterPasses[index(e.m_source, graph)];
                           raster.subPasses.emplace_back(static_cast<VertexType>(e.m_target));
                       },
                       [&](const auto &) {}
                   }, tag(e.m_source, graph));
    }

    void RenderGraphPassCompiler::discover_vertex(Vertex u, const Graph& g)
    {
        LOG_I(TAG, "compile passes %s....", name(u, graph).c_str());
        std::visit(Overloaded{
                       [&](const ComputePassTag &) {
                           auto &compute = graph.computePasses[index(u, graph)];
                       },
                       [&](const CopyBlitTag &) {
                           auto &cb = graph.copyBlitPasses[index(u, graph)];
                       },
                       [&](const PresentTag &) {
                           auto &present = graph.presentPasses[index(u, graph)];
                       },
                       [&](const auto &) {}
                   }, tag(u, graph));
    }

    void RenderDependencyCompiler::forward_or_cross_edge(Edge u, const Graph& g)
    {
        LOG_I(TAG, "dependency [%s] -> [%s]", name(u.m_source, graph).c_str(), name(u.m_target, graph).c_str());
    }
}

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
                                  auto &src = graph.images[graph.polymorphicDatas[u.m_source]];
                                  source    = src.res;
                              },
                              [&](const BufferTag &) {
                                  auto &src = graph.buffers[graph.polymorphicDatas[u.m_source]];
                                  source    = src.res;
                              },
                              [&](const ImageViewTag &) {
                                  auto &src = graph.imageViews[graph.polymorphicDatas[u.m_source]];
                                  source    = src.res;
                              },
                              [&](const BufferViewTag &) {
                                  auto &src = graph.bufferViews[graph.polymorphicDatas[u.m_source]];
                                  source    = src.res;
                              },
                              [&](const ImportImageTag &) {
                                  auto &src = graph.importImages[graph.polymorphicDatas[u.m_source]];
                                  source    = src.res;
                              },
                              [&](const ImportBufferTag &) {
                                  auto &src = graph.importBuffers[graph.polymorphicDatas[u.m_source]];
                                  source    = src.res;
                              },
                              [&](const auto &) {}},
                   graph.tags[u.m_source]);

        std::visit(Overloaded{[&](const rhi::ImageViewPtr &view) {
                                  auto &dst = graph.imageViews[graph.polymorphicDatas[u.m_target]];
                                  dst.res   = view->CreateView(dst.desc.view);
                              },
                              [&](const rhi::BufferViewPtr &view) {
                                  auto &dst = graph.bufferViews[graph.polymorphicDatas[u.m_target]];
                                  dst.res   = view->CreateView(dst.desc.view);
                              },
                              [&](const auto &) {}},
                   source);
    }

    void RenderResourceCompiler::discover_vertex(Vertex u, const Graph& g)
    {
        LOG_I(TAG, "compile resource %s....", graph.names[u].c_str());
        std::visit(Overloaded{
                       [&](const ImageTag &) {
                           auto &image = graph.images[graph.polymorphicDatas[u]];
                           image.res = graph.context->pool->requestImage(image.desc);
                       },
                       [&](const BufferTag &) {
                           auto &buffer = graph.buffers[graph.polymorphicDatas[u]];
                           buffer.res = graph.context->pool->requestBuffer(buffer.desc);
                       },
                       [&](const ImportImageTag &) {
                           auto &image = graph.importImages[graph.polymorphicDatas[u]];
                           auto &info = image.desc.image->GetDescriptor();

                           rhi::ImageViewDesc viewDesc = {};
                           viewDesc.subRange = {0, info.mipLevels, 0, info.arrayLayers};
                           viewDesc.mask = image.desc.mask;
                           viewDesc.viewType = image.desc.viewType;
                           image.res = image.desc.image->CreateView(viewDesc);
                       },
                       [&](const ImportBufferTag &) {
                           auto &buffer = graph.importBuffers[graph.polymorphicDatas[u]];
                           auto &info = buffer.desc.buffer->GetBufferDesc();

                           rhi::BufferViewDesc viewDesc = {};
                           viewDesc.offset = 0;
                           viewDesc.range  = info.size;
                           buffer.res = buffer.desc.buffer->CreateView(viewDesc);
                       },
                       [&](const auto &) {}
        }, graph.tags[u]);
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
        std::visit(
            Overloaded{[&](const RasterPassTag &) {
                           auto &raster = graph.rasterPasses[graph.polymorphicDatas[e.m_source]];
                           raster.subPasses.emplace_back(e.m_target);
                       },
                       [&](const auto &) {}},
            graph.tags[e.m_source]);
    }

    void RenderGraphPassCompiler::discover_vertex(Vertex u, const Graph& g)
    {
        LOG_I(TAG, "compile passes %s....", graph.names[u].c_str());
        std::visit(Overloaded{
                       [&](const ComputePassTag &) {
                           auto &compute = graph.computePasses[graph.polymorphicDatas[u]];
                       },
                       [&](const CopyBlitTag &) {
                           auto &cb = graph.copyBlitPasses[graph.polymorphicDatas[u]];
                       },
                       [&](const PresentTag &) {
                           auto &present = graph.presentPasses[graph.polymorphicDatas[u]];
                       },
                       [&](const auto &) {}
                   }, graph.tags[u]);
    }
}

//
// Created by Zach Lee on 2023/5/31.
//

#include <render/rdg/RenderResourceCompiler.h>
#include <render/rdg/RenderGraph.h>
#include <core/logger/Logger.h>

static const char *TAG = "RDG";

namespace sky::rdg {

    void RenderResourceCompiler::discover_vertex(Vertex u, const Graph& g)
    {
        std::visit(Overloaded{
            [&](const AccessPassTag &) {
                auto &pass = rdg.accessGraph.passes[Index(u, rdg.accessGraph)];
            },
            [&](const AccessResTag &) {
                auto &res = rdg.accessGraph.resources[Index(u, rdg.accessGraph)];
                MountResource(u, res.resID);
            },
            [&](const auto &) {}
        }, Tag(u, rdg.accessGraph));
    }

    void RenderResourceCompiler::MountResource(Vertex u, ResourceGraph::vertex_descriptor res)
    {
        std::visit(Overloaded{
            [&](const ImageTag &) {
                auto &image = rdg.resourceGraph.images[Index(res, rdg.resourceGraph)];

                if (!image.res && u >= image.lifeTime.begin && u < image.lifeTime.end) {
                    image.res = rdg.context->pool->RequestImage(image.desc);
                    LOG_I(TAG, "compile resource %s, lifeTime[%u, %u]...", Name(res, rdg.resourceGraph).c_str(),
                          image.lifeTime.begin, image.lifeTime.end);
                }
                if (image.res && u >= image.lifeTime.end) {
                    rdg.context->pool->RecycleImage(image.res, image.desc);
                    image.res = nullptr;
                }
            },
            [&](const BufferTag &) {
                auto &buffer = rdg.resourceGraph.buffers[Index(res, rdg.resourceGraph)];

                if (!buffer.res && u >= buffer.lifeTime.begin && u < buffer.lifeTime.end) {
                    buffer.res = rdg.resourceGraph.context->pool->RequestBuffer(buffer.desc);
                    LOG_I(TAG, "compile resource %s, lifeTime[%u, %u]...", Name(res, rdg.resourceGraph).c_str(),
                          buffer.lifeTime.begin, buffer.lifeTime.end);
                }
                if (buffer.res && u >= buffer.lifeTime.end) {
                    rdg.context->pool->RecycleBuffer(buffer.res, buffer.desc);
                    buffer.res = nullptr;
                }
            },
            [&](const ImportImageTag &) {
                auto &image = rdg.resourceGraph.importImages[Index(res, rdg.resourceGraph)];
                const auto &info = image.desc.image->GetDescriptor();

                if (!image.res) {
                    rhi::ImageViewDesc viewDesc = {};
                    viewDesc.subRange = {0, info.mipLevels, 0, info.arrayLayers, image.desc.mask};
                    viewDesc.viewType = image.desc.viewType;
                    image.res = image.desc.image->CreateView(viewDesc);
                }
            },
            [&](const ImportBufferTag &) {
                auto &buffer = rdg.resourceGraph.importBuffers[Index(res, rdg.resourceGraph)];
                const auto &info = buffer.desc.buffer->GetBufferDesc();

                if (!buffer.res) {
                    rhi::BufferViewDesc viewDesc = {};
                    viewDesc.offset = 0;
                    viewDesc.range = info.size;
                    buffer.res = buffer.desc.buffer->CreateView(viewDesc);
                }
            },
            [&](const auto &) {}
        }, Tag(res, rdg.resourceGraph));
    }

//    void ResourceGraphCompiler::tree_edge(Edge u, const Graph &g) {
//        std::variant<std::monostate, rhi::ImageViewPtr, rhi::BufferViewPtr> source;
//        std::visit(Overloaded{
//            [&](const ImageTag &) {
//                auto &src = graph.images[Index(u.m_source, graph)];
//                source = src.res;
//            },
//            [&](const BufferTag &) {
//                auto &src = graph.buffers[Index(u.m_source, graph)];
//                source = src.res;
//            },
//            [&](const ImageViewTag &) {
//                auto &src = graph.imageViews[Index(u.m_source, graph)];
//                source = src.res;
//            },
//            [&](const BufferViewTag &) {
//                auto &src = graph.bufferViews[Index(u.m_source, graph)];
//                source = src.res;
//            },
//            [&](const ImportImageTag &) {
//                auto &src = graph.importImages[Index(u.m_source, graph)];
//                source = src.res;
//            },
//            [&](const ImportBufferTag &) {
//                auto &src = graph.importBuffers[Index(u.m_source, graph)];
//                source = src.res;
//            },
//            [&](const auto &) {}
//        }, Tag(u.m_source, graph));
//
//        std::visit(Overloaded{
//            [&](const rhi::ImageViewPtr &view) {
//                auto &dst = graph.imageViews[Index(u.m_target, graph)];
//                dst.res = view->CreateView(dst.desc.view);
//            },
//            [&](const rhi::BufferViewPtr &view) {
//                auto &dst = graph.bufferViews[Index(u.m_target, graph)];
//                dst.res = view->CreateView(dst.desc.view);
//            },
//            [&](const auto &) {}
//        }, source);
//    }
//
//    void ResourceGraphCompiler::discover_vertex(Vertex u, const Graph& g) {
//        std::visit(Overloaded{
//            [&](const ImageTag &) {
//                auto &image = graph.images[Index(u, graph)];
//                image.res = graph.context->pool->requestImage(image.desc);
//                LOG_I(TAG, "compile resource %s, lifeTime[%u, %u]...", Name(u, graph).c_str(),
//                      image.lifeTime.begin, image.lifeTime.end);
//            },
//            [&](const BufferTag &) {
//                auto &buffer = graph.buffers[Index(u, graph)];
//                buffer.res = graph.context->pool->requestBuffer(buffer.desc);
//                LOG_I(TAG, "compile resource %s, lifeTime[%u, %u]...", Name(u, graph).c_str(),
//                      buffer.lifeTime.begin, buffer.lifeTime.end);
//            },
//            [&](const ImportImageTag &) {
//                auto &image = graph.importImages[Index(u, graph)];
//                const auto &info = image.desc.image->GetDescriptor();
//
//                rhi::ImageViewDesc viewDesc = {};
//                viewDesc.subRange = {0, info.mipLevels, 0, info.arrayLayers};
//                viewDesc.mask = image.desc.mask;
//                viewDesc.viewType = image.desc.viewType;
//                image.res = image.desc.image->CreateView(viewDesc);
//            },
//            [&](const ImportBufferTag &) {
//                auto &buffer = graph.importBuffers[Index(u, graph)];
//                const auto &info = buffer.desc.buffer->GetBufferDesc();
//
//                rhi::BufferViewDesc viewDesc = {};
//                viewDesc.offset = 0;
//                viewDesc.range = info.size;
//                buffer.res = buffer.desc.buffer->CreateView(viewDesc);
//            },
//            [&](const auto &) {}
//        }, Tag(u, graph));
//    }

} // namespace sky::rdg
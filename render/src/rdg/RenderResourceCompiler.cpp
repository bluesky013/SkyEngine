//
// Created by Zach Lee on 2023/5/31.
//

#include <render/rdg/RenderResourceCompiler.h>
#include <render/rdg/RenderGraph.h>
#include <core/logger/Logger.h>
#include <rhi/Decode.h>
#include <rhi/Device.h>

static const char *TAG = "RDG";

namespace sky::rdg {

    void RenderResourceCompiler::discover_vertex(Vertex u, const Graph& g)
    {
        std::visit(Overloaded{
            [&](const RasterPassTag &) {
                auto &raster = rdg.rasterPasses[Index(u, rdg)];
                Compile(u, raster);
            },
            [&](const RasterSubPassTag &) {
                auto &subPass = rdg.subPasses[Index(u, rdg)];
                Compile(u, subPass);
            },
            [&](const ComputePassTag &) {
                auto &compute = rdg.computePasses[Index(u, rdg)];
                Compile(u, compute);
            },
            [&](const CopyBlitTag &) {
                auto &cb = rdg.copyBlitPasses[Index(u, rdg)];
                Compile(u, cb);
            },
            [&](const PresentTag &) {
                auto &present = rdg.presentPasses[Index(u, rdg)];
            },
            [&](const auto &) {}
        }, Tag(u, rdg));
    }

    void RenderResourceCompiler::Compile(Vertex u, RasterPass &rasterPass)
    {
        for (auto &attachment : rasterPass.attachmentVertex) {
            MountResource(u, Source(attachment, rdg.resourceGraph));
        }
        CreateRenderPassAndFramebuffer(u, rasterPass);
    }

    void RenderResourceCompiler::Compile(Vertex u, RasterSubPass &subPass)
    {
        for (auto &[name, compute] : subPass.computeViews) {
            MountResource(u, Source(FindVertex(name.c_str(), rdg.resourceGraph), rdg.resourceGraph));
        }
    }

    void RenderResourceCompiler::Compile(Vertex u, ComputePass &pass)
    {

    }

    void RenderResourceCompiler::Compile(Vertex u, CopyBlitPass &pass)
    {

    }

    void RenderResourceCompiler::MountResource(Vertex u, ResourceGraph::vertex_descriptor res)
    {
        std::visit(Overloaded{
            [&](const ImageTag &) {
                auto &image = rdg.resourceGraph.images[Index(res, rdg.resourceGraph)];

                if (!image.res && u >= image.lifeTime.begin && u <= image.lifeTime.end) {
                    image.res = rdg.context->pool->RequestImage(image.desc);
                    LOG_I(TAG, "compile resource %s, lifeTime[%u, %u]...", Name(res, rdg.resourceGraph).c_str(),
                          image.lifeTime.begin, image.lifeTime.end);
                }
            },
            [&](const BufferTag &) {
                auto &buffer = rdg.resourceGraph.buffers[Index(res, rdg.resourceGraph)];

                if (!buffer.res && u >= buffer.lifeTime.begin && u <= buffer.lifeTime.end) {
                    buffer.res = rdg.resourceGraph.context->pool->RequestBuffer(buffer.desc);
                    LOG_I(TAG, "compile resource %s, lifeTime[%u, %u]...", Name(res, rdg.resourceGraph).c_str(),
                          buffer.lifeTime.begin, buffer.lifeTime.end);
                }
            },
            [&](const ImportImageTag &) {
                auto &image = rdg.resourceGraph.importImages[Index(res, rdg.resourceGraph)];
                const auto &info = image.desc.image->GetDescriptor();

                if (!image.res) {
                    rhi::ImageViewDesc viewDesc = {};
                    viewDesc.subRange = {0, info.mipLevels, 0, info.arrayLayers, GetAspectFlagsByFormat(info.format)};
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

    void RenderResourceCompiler::CreateRenderPassAndFramebuffer(Vertex u, RasterPass &rasterPass)
    {
        rhi::RenderPass::Descriptor passDesc = {};
        rhi::FrameBuffer::Descriptor fbDesc = {};

        auto attachmentSize = rasterPass.attachmentVertex.size();
        auto subPassSize = rasterPass.subPasses.size();
        auto dependencySize = rasterPass.dependencies.size();

        passDesc.attachments.resize(attachmentSize);
        fbDesc.views.resize(attachmentSize);

        for (uint32_t i = 0; i < attachmentSize; ++i) {
            auto &attachment = rasterPass.attachmentVertex[i];
            auto &attachmentDesc = rasterPass.attachments[i];

            auto &source = rdg.resourceGraph.images[Index(Source(attachment, rdg.resourceGraph), rdg.resourceGraph)];
            auto &view = rdg.resourceGraph.imageViews[Index(attachment, rdg.resourceGraph)];
            if (!view.res) {
                view.res = source.res->CreateView(view.desc.view);
            }
            fbDesc.views[i] = view.res;
            auto &att = passDesc.attachments[i];
            att.format = source.desc.format;
            att.sample = source.desc.samples;
            att.load = attachmentDesc.loadOp;
            att.store = attachmentDesc.storeOp;
            att.stencilLoad = attachmentDesc.stencilLoad;
            att.stencilStore = attachmentDesc.stencilStore;
        }

        passDesc.subPasses.resize(subPassSize);
        for (uint32_t i = 0; i < subPassSize; ++i) {
            auto &sub = passDesc.subPasses[i];
            auto &subVtx = rdg.subPasses[Index(rasterPass.subPasses[i], rdg)];

            for (auto &input : subVtx.inputs) {
                auto &ref = sub.inputs.emplace_back();
                ref.index = input.index;
                auto &attachmentDesc = rasterPass.attachments[ref.index];
                ref.access = subVtx.rasterViews.at(attachmentDesc.name).type == (RasterTypeBit::INPUT | RasterTypeBit::COLOR) ?
                    rhi::AccessFlagBit::COLOR_INOUT_WRITE : rhi::AccessFlagBit::COLOR_WRITE;
            }

            for (auto &color : subVtx.colors) {
                auto &ref = sub.colors.emplace_back();
                ref.index = color.index;
                auto &attachmentDesc = rasterPass.attachments[ref.index];
                ref.access = subVtx.rasterViews.at(attachmentDesc.name).type == (RasterTypeBit::INPUT | RasterTypeBit::COLOR) ?
                    rhi::AccessFlagBit::COLOR_INOUT_WRITE : rhi::AccessFlagBit::COLOR_WRITE;
            }

            for (auto &resolve : subVtx.resolves) {
                auto &ref = sub.resolves.emplace_back();
                ref.index = resolve.index;
                ref.access = rhi::AccessFlagBit::COLOR_WRITE;
            }

            if (subVtx.depthStencil.index != INVALID_INDEX) {
                sub.depthStencil.index = subVtx.depthStencil.index;
                auto &attachmentDesc = rasterPass.attachments[subVtx.depthStencil.index];
                sub.depthStencil.access = subVtx.rasterViews.at(attachmentDesc.name).type == (RasterTypeBit::INPUT | RasterTypeBit::DEPTH_STENCIL) ?
                    rhi::AccessFlagBit::DEPTH_STENCIL_INOUT_WRITE : rhi::AccessFlagBit::DEPTH_STENCIL_WRITE;
            }
        }

        passDesc.dependencies.resize(dependencySize);
        for (uint32_t i = 0; i < dependencySize; ++i) {
            auto &dep = passDesc.dependencies[i];
            auto &depVtx = rasterPass.dependencies[i];

            dep.src = depVtx.src;
            dep.dst = depVtx.dst;
            dep.srcAccess = depVtx.preAccess;
            dep.dstAccess = depVtx.nextAccess;
        }
        auto pass = rdg.context->device->CreateRenderPass(passDesc);
        fbDesc.pass = pass;
    }
} // namespace sky::rdg

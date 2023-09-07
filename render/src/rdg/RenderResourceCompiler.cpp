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
            [&](const RasterQueueTag &) {
                auto &queue = rdg.rasterPasses[Index(u, rdg)];
                Compile(u, queue);
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

    static void BindResourceGroup(const RenderGraph &rdg, const PmrHashMap<std::string, ComputeView> &computeViews, const RDResourceGroupPtr &rsg)
    {
        const auto &layout = rsg->GetLayout();
        for (const auto &[name, computeView] : computeViews) {
            auto res = FindVertex(name.c_str(), rdg.resourceGraph);
            auto &view = computeView;
            std::visit(Overloaded{
                [&](const ImageTag &) {
                    auto &image = rdg.resourceGraph.images[Index(res, rdg.resourceGraph)];
                    rsg->BindTexture(view.name, image.res, 0);
                },
                [&](const ImageViewTag &) {
                    auto &imageView = rdg.resourceGraph.imageViews[Index(res, rdg.resourceGraph)];
                    rsg->BindTexture(view.name, imageView.res, 0);
                },
                [&](const BufferTag &) {
                    auto &buffer = rdg.resourceGraph.buffers[Index(res, rdg.resourceGraph)];
                },
                [&](const ImportImageTag &) {
                    auto &image = rdg.resourceGraph.importImages[Index(res, rdg.resourceGraph)];
                    rsg->BindTexture(view.name, image.res, 0);
                },
                [&](const ImportSwapChainTag &) {
                    auto &swc = rdg.resourceGraph.swapChains[Index(res, rdg.resourceGraph)];
                    rsg->BindTexture(view.name, swc.res, 0);
                },
                [&](const auto &) {}
            }, Tag(res, rdg.resourceGraph));
        }
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
        for (auto &[name, compute] : pass.computeViews) {
            MountResource(u, Source(FindVertex(name.c_str(), rdg.resourceGraph), rdg.resourceGraph));
        }
        BindResourceGroup(rdg, pass.computeViews, pass.resourceGroup);
    }

    void RenderResourceCompiler::Compile(Vertex u, CopyBlitPass &pass)
    {

    }

    void RenderResourceCompiler::Compile(unsigned long u, sky::rdg::RasterQueue &queue)
    {
        const auto &subPass = rdg.subPasses[Index(queue.passID, rdg)];
        BindResourceGroup(rdg, subPass.computeViews, queue.resourceGroup);
    }

    void RenderResourceCompiler::MountResource(Vertex u, ResourceGraph::vertex_descriptor res)
    {
        std::visit(Overloaded{
            [&](const ImageTag &) {
                auto &image = rdg.resourceGraph.images[Index(res, rdg.resourceGraph)];

                if (!image.desc.image && u >= image.lifeTime.begin && u <= image.lifeTime.end) {
                    auto *imageObject = rdg.context->pool->RequestImage(image.desc);
                    image.desc.image = imageObject->image;
                    const auto &imageDesc = image.desc;
                    rhi::ImageViewDesc viewDesc = {};
                    viewDesc.subRange = {0, imageDesc.mipLevels, 0, imageDesc.arrayLayers, GetAspectFlagsByFormat(imageDesc.format)};
                    viewDesc.viewType = image.desc.viewType;
                    image.res = imageObject->RequestView(viewDesc);
                }
            },
            [&](const BufferTag &) {
                auto &buffer = rdg.resourceGraph.buffers[Index(res, rdg.resourceGraph)];

                if (!buffer.desc.buffer && u >= buffer.lifeTime.begin && u <= buffer.lifeTime.end) {
                    auto *bufferObject = rdg.context->pool->RequestBuffer(buffer.desc);
                    buffer.desc.buffer = bufferObject->buffer;
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
            [&](const ImportSwapChainTag &) {
                auto &swc = rdg.resourceGraph.swapChains[Index(res, rdg.resourceGraph)];
                if (!swc.res) {
                    swc.desc.imageIndex = swc.desc.swapchain->AcquireNextImage(rdg.context->ImageAvailableSemaPool().Acquire());
                    swc.res = swc.desc.swapchain->GetImageView(swc.desc.imageIndex);
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
        fbDesc.extent.width = rasterPass.width;
        fbDesc.extent.height = rasterPass.height;

        for (uint32_t i = 0; i < attachmentSize; ++i) {
            auto &attachment = rasterPass.attachmentVertex[i];
            auto &attachmentDesc = rasterPass.attachments[i];
            auto &att = passDesc.attachments[i];

            std::visit(Overloaded{
                [&](const ImageTag &) {
                    auto &image = rdg.resourceGraph.images[Index(attachment, rdg.resourceGraph)];
                    att.format = image.desc.format;
                    att.sample = image.desc.samples;

                    fbDesc.views[i] = image.res;
                },
                [&](const ImportImageTag &) {
                    auto &image = rdg.resourceGraph.importImages[Index(attachment, rdg.resourceGraph)];
                    const auto &imageDesc = image.desc.image->GetDescriptor();
                    att.format = imageDesc.format;
                    att.sample = imageDesc.samples;

                    fbDesc.views[i] = image.res;
                },
                [&](const ImportSwapChainTag &) {
                    auto &swc = rdg.resourceGraph.swapChains[Index(attachment, rdg.resourceGraph)];
                    att.format = swc.desc.swapchain->GetFormat();

                    fbDesc.views[i] = swc.res;
                },
                [&](const ImageViewTag &) {
                    auto &source = rdg.resourceGraph.images[Index(Source(attachment, rdg.resourceGraph), rdg.resourceGraph)];
                    att.format = source.desc.format;
                    att.sample = source.desc.samples;
                    auto &view = rdg.resourceGraph.imageViews[Index(attachment, rdg.resourceGraph)];
                    if (!view.res) {
                        view.res = source.res->CreateView(view.desc.view);
                    }
                    fbDesc.views[i] = view.res;
                },
                [&](const auto &) {}
            }, Tag(attachment, rdg.resourceGraph));

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
        rasterPass.renderPass = rdg.context->device->CreateRenderPass(passDesc);
        fbDesc.pass = rasterPass.renderPass;
        rasterPass.frameBuffer = rdg.context->pool->RequestFrameBuffer(fbDesc);
    }
} // namespace sky::rdg

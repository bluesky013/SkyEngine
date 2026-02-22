//
// Created by Zach Lee on 2023/3/27.
//

#pragma once

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/properties.hpp>
#include <boost/property_map/property_map.hpp>

#include <core/template/Overloaded.h>
#include <core/platform/Platform.h>
#include <core/name/Name.h>
#include <render/rdg/RenderGraphContext.h>
#include <render/rdg/RenderGraphTypes.h>

namespace sky {
    class RenderScene;
} // namespace sky

namespace sky::rdg {
    struct RenderGraph;

    struct RasterQueueBuilder {
        RasterQueueBuilder &SetRasterID(const Name &id);
        RasterQueueBuilder &SetLayout(const RDResourceLayoutPtr &layout);
        RasterQueueBuilder &SetSceneView(const Name& view);

        RenderGraph &rdg;
        RasterQueue &queue;
        VertexType vertex;
    };

    struct FullScreenBuilder {
        FullScreenBuilder &SetTechnique(const RDGfxTechPtr &tech);

        RenderGraph &rdg;
        FullScreenBlit &fullscreen;
        VertexType vertex;
    };

    struct RasterSubPassBuilder {
        RasterSubPassBuilder &AddColor(const Name &name, const ResourceAccess& access);
        RasterSubPassBuilder &AddResolve(const Name &name, const ResourceAccess& access);
        RasterSubPassBuilder &AddInput(const Name &name, const ResourceAccess& access);
        RasterSubPassBuilder &AddColorInOut(const Name &name);
        RasterSubPassBuilder &AddDepthStencil(const Name &name, const ResourceAccess& access);
        RasterSubPassBuilder &AddComputeView(const Name &name, const ComputeView &view);
        RasterSubPassBuilder &AddSamplerView(const Name &name, const Name& viewName);
        RasterSubPassBuilder &SetViewMask(uint32_t mask);
        RasterQueueBuilder AddQueue(const Name &name);
        FullScreenBuilder AddFullScreen(const Name &name);
        uint32_t GetAttachmentIndex(const Name &name);

        RenderGraph &rdg;
        RasterPass &pass;
        RasterSubPass &subPass;
        VertexType vertex;

    protected:
        RasterSubPassBuilder &AddRasterView(const Name &name, VertexType resVertex, const RasterView &view);
    };

    struct RasterPassBuilder {
        RasterPassBuilder &AddAttachment(const RasterAttachment &attachment, const rhi::ClearValue &clear = rhi::ClearValue(0, 0, 0, 0));
        RasterPassBuilder &AddCoRelationMasks(uint32_t mask);
        RasterSubPassBuilder AddRasterSubPass(const Name &name);

        RenderGraph &rdg;
        RasterPass &pass;
        VertexType vertex;
    };

    struct ComputePassBuilder {
        ComputePassBuilder &AddComputeView(const Name &name, const ComputeView &view);

        RenderGraph &rdg;
        ComputePass &compute;
        VertexType vertex;
    };

    struct CopyPassBuilder {
        CopyPassBuilder &AddCopyView(const CopyView &view);

        RenderGraph &rdg;
        CopyBlitPass &blit;
        VertexType vertex;
    };

    struct AccessGraph {
        explicit AccessGraph(RenderGraphContext *ctx);
        ~AccessGraph() = default;

        using Graph = boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, boost::no_property>;
        using vertex_descriptor = VertexType;
        using Tag = AccessGraphTags;

        // memory
        RenderGraphContext *context;

        // vertex
        VertexList vertices;

        // components
        PmrVector<Tag>        tags;
        PmrVector<size_t>     polymorphicDatas;

        // resources
        PmrVector<AccessPass> passes;
        PmrVector<AccessRes>  resources;

        Graph graph;
    };

    struct ResourceGraph {
        explicit ResourceGraph(RenderGraphContext *ctx);
        ~ResourceGraph() = default;

        using Graph = boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS>;
        using vertex_descriptor = VertexType;
        using Tag   = ResourceGraphTags;

        void AddImage(const Name &name, const GraphImage &image);

        void ImportImage(const Name &name, const rhi::ImagePtr &image);
        void ImportImage(const Name &name, const rhi::ImagePtr &image, rhi::ImageViewType viewType);
        void ImportImage(const Name &name, const rhi::ImagePtr &image, rhi::ImageViewType viewType, const rhi::AccessFlags &flags);

        void ImportImageView(const Name &name, const rhi::ImagePtr &image, const rhi::ImageViewPtr &view, const rhi::AccessFlags &flags);

        void ImportSwapChain(const Name &name, const rhi::SwapChainPtr &swapchain);
#ifdef SKY_ENABLE_XR
        void ImportXRSwapChain(const Name &name, const rhi::XRSwapChainPtr &swapchain);
#endif
        void AddImageView(const Name &name, const Name &source, const GraphImageView &view);

        void AddBuffer(const Name &name, const GraphBuffer &attachment);

        void ImportBuffer(const Name &name, const rhi::BufferPtr &buffer);
        void ImportBuffer(const Name &name, const rhi::BufferPtr &buffer, const rhi::AccessFlags &flags);
        void ImportUBO(const Name &name, const RDUniformBufferPtr &ubo);
        void ImportSampler(const Name &name, const rhi::SamplerPtr &sampler);

        // memory
        RenderGraphContext *context;

        // vertex
        VertexList vertices;

        // components
        PmrVector<Name>       names;
        PmrVector<VertexType> lastAccesses;
        PmrVector<Tag>        tags;
        PmrVector<size_t>     polymorphicDatas;

        // resources
        PmrVector<ImageViewRes<GraphImage>>           images;
        PmrVector<ImageViewRes<GraphImportImage>>     importImages;
        PmrVector<ImageViewRes<GraphImageView>>       imageViews;
        PmrVector<ImageViewRes<GraphSwapChain>>       swapChains;
        PmrVector<ImageViewRes<GraphImportImageView>> importedViews;
        PmrVector<ImageViewRes<GraphXRSwapChain>>     xrSwapChains;
        PmrVector<BufferViewRes<GraphBuffer>>         buffers;
        PmrVector<BufferViewRes<GraphImportBuffer>>   importBuffers;
        PmrVector<BufferViewRes<GraphBufferView>>     bufferViews;
        PmrVector<GraphConstantBuffer>                constantBuffers;
        PmrVector<GraphSampler>                       samplers;

        Graph graph;
    };

    struct RenderGraph {
        explicit RenderGraph(RenderGraphContext *ctx);
        ~RenderGraph() = default;

        using vertex_descriptor = VertexType;
        using Graph = boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS>;
        using Tag = RenderGraphTags;

        RasterPassBuilder  AddRasterPass(const Name &name, uint32_t width, uint32_t height);
        ComputePassBuilder AddComputePass(const Name &name);
        CopyPassBuilder    AddCopyPass(const Name &name);
        void               AddSceneView(const Name& name, SceneView* view);

        void AddUploadPass(const Name &name, const UploadPass &upload);
        void AddPresentPass(const Name &name, const Name &resName);
        void AddTransitionPass(const Name &name, const Name &resName, const DependencyInfo& deps);
        void AddDependency(VertexType resVertex, VertexType passId, const DependencyInfo &deps);

        static bool CheckVersionChanged(const AccessRes &lastAccess, const DependencyInfo &deps);

        // memory
        RenderGraphContext *context;
//        RenderScene *scene;

        // vertex
        VertexList vertices;

        // components
        PmrVector<Name>       names;
        PmrVector<VertexType> accessNodes;
        PmrVector<Tag>        tags;
        PmrVector<size_t>     polymorphicDatas;

        // passes
        PmrVector<RasterPass>     rasterPasses;
        PmrVector<RasterSubPass>  subPasses;
        PmrVector<RasterQueue>    rasterQueues;
        PmrVector<FullScreenBlit> fullScreens;
        PmrVector<RdgSceneView>   sceneViews;

        PmrVector<ComputePass>    computePasses;
        PmrVector<CopyBlitPass>   copyBlitPasses;
        PmrVector<PresentPass>    presentPasses;
        PmrVector<UploadPass>     uploadPasses;
        PmrVector<TransitionPass> transitionPasses;

        ResourceGraph   resourceGraph;
        AccessGraph     accessGraph;
        Graph graph;
    };

    template <typename D>
    VertexType AddVertex(D &&val, AccessGraph &graph)
    {
        using Tag = typename std::remove_reference<D>::type::Tag;
        auto vertex = static_cast<VertexType>(graph.vertices.size());
        graph.vertices.emplace_back();
        graph.tags.emplace_back(Tag{});

        if constexpr (std::is_same_v<Tag, AccessPassTag>) {
            graph.polymorphicDatas.emplace_back(graph.passes.size());
            graph.passes.emplace_back(std::forward<D>(val));
        } else if constexpr (std::is_same_v<Tag, AccessResTag>) {
            graph.polymorphicDatas.emplace_back(graph.resources.size());
            graph.resources.emplace_back(std::forward<D>(val));
        } else {
            graph.polymorphicDatas.emplace_back(0);
        }

        return vertex;
    }

    template <typename D>
    VertexType AddVertex(const Name &name, D &&val, ResourceGraph &graph)
    {
        using Tag   = typename std::remove_reference<D>::type::Tag;
        auto vertex = static_cast<VertexType>(graph.vertices.size());
        graph.vertices.emplace_back();
        graph.tags.emplace_back(Tag{});
        graph.lastAccesses.emplace_back(INVALID_VERTEX);
        graph.names.emplace_back(name);

        if constexpr (std::is_same_v<Tag, ImageTag>) {
            graph.polymorphicDatas.emplace_back(graph.images.size());
            graph.images.emplace_back(std::forward<D>(val));
        } else if constexpr (std::is_same_v<Tag, ImageViewTag>) {
            graph.polymorphicDatas.emplace_back(graph.imageViews.size());
            graph.imageViews.emplace_back(std::forward<D>(val));
        } else if constexpr (std::is_same_v<Tag, ImportImageTag>) {
            graph.polymorphicDatas.emplace_back(graph.importImages.size());
            graph.importImages.emplace_back(std::forward<D>(val));
        } else if constexpr (std::is_same_v<Tag, ImportSwapChainTag>) {
            graph.polymorphicDatas.emplace_back(graph.swapChains.size());
            graph.swapChains.emplace_back(std::forward<D>(val));
        } else if constexpr (std::is_same_v<Tag, ImportImageViewTag>) {
            graph.polymorphicDatas.emplace_back(graph.importedViews.size());
            graph.importedViews.emplace_back(std::forward<D>(val));
        } else if constexpr (std::is_same_v<Tag, ImportXRSwapChainTag>) {
            graph.polymorphicDatas.emplace_back(graph.xrSwapChains.size());
            graph.xrSwapChains.emplace_back(std::forward<D>(val));
        } else if constexpr (std::is_same_v<Tag, BufferTag>) {
            graph.polymorphicDatas.emplace_back(graph.buffers.size());
            graph.buffers.emplace_back(std::forward<D>(val));
        } else if constexpr (std::is_same_v<Tag, ImportBufferTag>) {
            graph.polymorphicDatas.emplace_back(graph.importBuffers.size());
            graph.importBuffers.emplace_back(std::forward<D>(val));
        } else if constexpr (std::is_same_v<Tag, BufferViewTag>) {
            graph.polymorphicDatas.emplace_back(graph.bufferViews.size());
            graph.bufferViews.emplace_back(std::forward<D>(val));
        } else if constexpr (std::is_same_v<Tag, ConstantBufferTag>) {
            graph.polymorphicDatas.emplace_back(graph.constantBuffers.size());
            graph.constantBuffers.emplace_back(std::forward<D>(val));
        } else if constexpr (std::is_same_v<Tag, SamplerTag>) {
            graph.polymorphicDatas.emplace_back(graph.samplers.size());
            graph.samplers.emplace_back(std::forward<D>(val));
        } else {
            graph.polymorphicDatas.emplace_back(0);
        }

        return vertex;
    }

    template <typename D>
    VertexType AddVertex(const Name &name, D &&val, RenderGraph &graph)
    {
        using Tag = typename std::remove_reference<D>::type::Tag;
        auto vertex = static_cast<VertexType>(graph.vertices.size());

        // access
        auto accessVtx = AddVertex(AccessPass{vertex}, graph.accessGraph);

        // components
        graph.vertices.emplace_back();
        graph.tags.emplace_back(Tag{});
        graph.accessNodes.emplace_back(accessVtx);
        graph.names.emplace_back(name);

        if constexpr (std::is_same_v<Tag, RasterPassTag>) {
            graph.polymorphicDatas.emplace_back(graph.rasterPasses.size());
            graph.rasterPasses.emplace_back(std::forward<D>(val));
        } else if constexpr (std::is_same_v<Tag, RasterSubPassTag>) {
            graph.polymorphicDatas.emplace_back(graph.subPasses.size());
            graph.subPasses.emplace_back(std::forward<D>(val));
        } else if constexpr (std::is_same_v<Tag, ComputePassTag>) {
            graph.polymorphicDatas.emplace_back(graph.computePasses.size());
            graph.computePasses.emplace_back(std::forward<D>(val));
        } else if constexpr (std::is_same_v<Tag, CopyBlitTag>) {
            graph.polymorphicDatas.emplace_back(graph.copyBlitPasses.size());
            graph.copyBlitPasses.emplace_back(std::forward<D>(val));
        } else if constexpr (std::is_same_v<Tag, PresentTag>) {
            graph.polymorphicDatas.emplace_back(graph.presentPasses.size());
            graph.presentPasses.emplace_back(std::forward<D>(val));
        } else if constexpr (std::is_same_v<Tag, RasterQueueTag>) {
            graph.polymorphicDatas.emplace_back(graph.rasterQueues.size());
            graph.rasterQueues.emplace_back(std::forward<D>(val));
        } else if constexpr (std::is_same_v<Tag, FullScreenBlitTag>) {
            graph.polymorphicDatas.emplace_back(graph.fullScreens.size());
            graph.fullScreens.emplace_back(std::forward<D>(val));
        } else if constexpr (std::is_same_v<Tag, UploadTag>) {
            graph.polymorphicDatas.emplace_back(graph.uploadPasses.size());
            graph.uploadPasses.emplace_back(std::forward<D>(val));
        } else if constexpr (std::is_same_v<Tag, TransitionTag>) {
            graph.polymorphicDatas.emplace_back(graph.transitionPasses.size());
            graph.transitionPasses.emplace_back(std::forward<D>(val));
        } else if constexpr (std::is_same_v<Tag, SceneViewTag>) {
            graph.polymorphicDatas.emplace_back(graph.sceneViews.size());
            graph.sceneViews.emplace_back(std::forward<D>(val));
        } else {
            graph.polymorphicDatas.emplace_back(0);
        }
        return vertex;
    }

    template <typename Graph>
    VertexType FindVertex(const Name &name, Graph &g)
    {
        auto iter = std::find(g.names.begin(), g.names.end(), name);
        return iter == g.names.end() ? INVALID_VERTEX : static_cast<VertexType>(std::distance(g.names.begin(), iter));
    }

    // component visitors
    template <typename V, typename G>
    Name &GetName(V v, G &g)
    {
        return g.names[static_cast<typename G::vertex_descriptor>(v)];
    }

    template <typename V, typename G>
    const typename G::Tag &Tag(V v, const G &g)
    {
        return g.tags[static_cast<typename G::vertex_descriptor>(v)];
    }

    template <typename V, typename G>
    size_t Index(V v, G &g)
    {
        return g.polymorphicDatas[static_cast<typename G::vertex_descriptor>(v)];
    }
} // namespace sky

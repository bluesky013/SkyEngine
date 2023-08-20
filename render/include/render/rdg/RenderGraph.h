//
// Created by Zach Lee on 2023/3/27.
//

#pragma once

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/properties.hpp>
#include <boost/property_map/property_map.hpp>

#include <core/template/Overloaded.h>
#include <core/platform/Platform.h>
#include <render/rdg/RenderGraphContext.h>
#include <render/rdg/RenderGraphTypes.h>

namespace sky::rdg {
    struct RenderGraph;

    struct RasterSubPassBuilder {
        RasterSubPassBuilder &AddColor(const std::string &name, const ResourceAccess& access);
        RasterSubPassBuilder &AddResolve(const std::string &name, const ResourceAccess& access);
        RasterSubPassBuilder &AddInput(const std::string &name, const ResourceAccess& access);
        RasterSubPassBuilder &AddColorInOut(const std::string &name);
        RasterSubPassBuilder &AddDepthStencil(const std::string &name, const ResourceAccess& access);
        RasterSubPassBuilder &AddComputeView(const std::string &name, const ComputeView &view);
        RasterSubPassBuilder &AddSceneView(const std::string &name, const SceneView *sceneView);
        uint32_t GetAttachmentIndex(const std::string &name);

        RenderGraph &rdg;
        RasterPass &pass;
        RasterSubPass &subPass;
        VertexType vertex;

    protected:
        RasterSubPassBuilder &AddRasterView(const std::string &name, VertexType resVertex, const RasterView &view);
    };

    struct RasterPassBuilder {
        RasterPassBuilder &AddAttachment(const RasterAttachment &attachment, const rhi::ClearValue &clear = rhi::ClearValue(0, 0, 0, 0));
        RasterSubPassBuilder AddRasterSubPass(const std::string &name);

        RenderGraph &rdg;
        RasterPass &pass;
        VertexType vertex;
    };

    struct ComputePassBuilder {
        ComputePassBuilder &AddComputeView(const std::string &name, const ComputeView &view);

        RenderGraph &graph;
        VertexType vertex;
    };

    struct CopyPassBuilder {
        CopyPassBuilder &AddCopyView(const CopyView &view);

        RenderGraph &graph;
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

        void AddImage(const char *name, const GraphImage &image);
        void ImportImage(const char *name, const rhi::ImagePtr &image);
        void ImportSwapChain(const char *name, const rhi::SwapChainPtr &swapchain);
        void AddImageView(const char *name, const char *source, const GraphImageView &view);

        void AddBuffer(const char *name, const GraphBuffer &attachment);
        void ImportBuffer(const char *name, const rhi::BufferPtr &buffer);
        void AddBufferView(const char *name, const char *source, const GraphBufferView &view);

        // memory
        RenderGraphContext *context;

        // vertex
        VertexList vertices;

        // components
        PmrVector<PmrString>  names;
        PmrVector<VertexType> sources;
        PmrVector<VertexType> lastAccesses;
        PmrVector<Tag>        tags;
        PmrVector<size_t>     polymorphicDatas;

        // resources
        PmrVector<ImageViewRes<GraphImage>>         images;
        PmrVector<ImageViewRes<GraphImportImage>>   importImages;
        PmrVector<ImageViewRes<GraphImageView>>     imageViews;
        PmrVector<ImageViewRes<GraphSwapChain>>     swapChains;
        PmrVector<BufferViewRes<GraphBuffer>>       buffers;
        PmrVector<BufferViewRes<GraphImportBuffer>> importBuffers;
        PmrVector<BufferViewRes<GraphBufferView>>   bufferViews;

        Graph graph;
    };

    struct RenderGraph {
        explicit RenderGraph(RenderGraphContext *ctx);
        ~RenderGraph() = default;

        using vertex_descriptor = VertexType;
        using Graph = boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS>;
        using Tag = RenderGraphTags;

        RasterPassBuilder    AddRasterPass(const char *name, uint32_t width, uint32_t height);
        ComputePassBuilder   AddComputePass(const char *name);
        CopyPassBuilder      AddCopyPass(const char *name);

        void AddPresentPass(const char *name, const char *resName);

        static bool CheckVersionChanged(const AccessRes &lastAccess, const DependencyInfo &deps, const AccessRange &subRange);

        void AddDependency(VertexType resVertex, VertexType passId, const DependencyInfo &deps);
        void FillAccessFlag(AccessRes &res, const AccessRange &subRange, const rhi::AccessFlags& accessFlag) const;
        AccessRes GetMergedAccessRes(const AccessRes &lastAccess,
                                     const rhi::AccessFlags& accessFlag,
                                     const AccessRange &subRange,
                                     VertexType passAccessID,
                                     VertexType nextAccessResID) const;

        // memory
        RenderGraphContext *context;

        // vertex
        VertexList vertices;

        // components
        PmrVector<PmrString>  names;
        PmrVector<VertexType> accessNodes;
        PmrVector<Tag>        tags;
        PmrVector<size_t>     polymorphicDatas;

        // passes
        PmrVector<RasterPass>      rasterPasses;
        PmrVector<RasterSubPass>   subPasses;
        PmrVector<RasterSceneView> sceneViews;

        PmrVector<ComputePass>   computePasses;
        PmrVector<CopyBlitPass>  copyBlitPasses;
        PmrVector<PresentPass>   presentPasses;

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
    VertexType AddVertex(const char *name, D &&val, ResourceGraph &graph)
    {
        using Tag = typename std::remove_reference<D>::type::Tag;
        auto vertex = static_cast<VertexType>(graph.vertices.size());
        graph.vertices.emplace_back();
        graph.tags.emplace_back(Tag{});
        graph.sources.emplace_back(vertex);
        graph.lastAccesses.emplace_back(INVALID_VERTEX);
        graph.names.emplace_back(PmrString(name, &graph.context->resources));

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
        } else if constexpr (std::is_same_v<Tag, BufferTag>) {
            graph.polymorphicDatas.emplace_back(graph.buffers.size());
            graph.buffers.emplace_back(std::forward<D>(val));
        } else if constexpr (std::is_same_v<Tag, ImportBufferTag>) {
            graph.polymorphicDatas.emplace_back(graph.importBuffers.size());
            graph.importBuffers.emplace_back(std::forward<D>(val));
        } else if constexpr (std::is_same_v<Tag, BufferViewTag>) {
            graph.polymorphicDatas.emplace_back(graph.bufferViews.size());
            graph.bufferViews.emplace_back(std::forward<D>(val));
        } else {
            graph.polymorphicDatas.emplace_back(0);
        }

        return vertex;
    }

    template <typename D>
    VertexType AddVertex(const char *name, D &&val, RenderGraph &graph)
    {
        using Tag = typename std::remove_reference<D>::type::Tag;
        auto vertex = static_cast<VertexType>(graph.vertices.size());

        // access
        auto accessVtx = AddVertex(AccessPass{vertex}, graph.accessGraph);

        // components
        graph.vertices.emplace_back();
        graph.tags.emplace_back(Tag{});
        graph.accessNodes.emplace_back(accessVtx);
        graph.names.emplace_back(PmrString(name, &graph.context->resources));

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
        } else if constexpr (std::is_same_v<Tag, RasterSceneViewTag>) {
            graph.polymorphicDatas.emplace_back(graph.sceneViews.size());
            graph.sceneViews.emplace_back(std::forward<D>(val));
        } else {
            graph.polymorphicDatas.emplace_back(0);
        }
        return vertex;
    }

    template <typename Graph>
    VertexType FindVertex(const char *name, Graph &g)
    {
        auto iter = std::find(g.names.begin(), g.names.end(), name);
        return iter == g.names.end() ? INVALID_VERTEX : static_cast<VertexType>(std::distance(g.names.begin(), iter));
    }

    // component visitors
    template <typename V, typename G>
    PmrString &Name(V v, G &g)
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

    template <typename V, typename G>
    VertexType Source(V v, G &g)
    {
        return g.sources[static_cast<typename G::vertex_descriptor>(v)];
    }
} // namespace sky

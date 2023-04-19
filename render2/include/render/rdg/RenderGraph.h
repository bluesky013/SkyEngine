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

    struct RasterPassBuilder {
        RenderGraph &graph;
        RasterPass &pass;
        VertexType vertex;
    };

    struct RasterSubPassBuilder {
        RasterSubPassBuilder &AddRasterView(const char *name, const RasterView &view);
        RasterSubPassBuilder &AddComputeView(const char *name, const ComputeView &view);

        RenderGraph &graph;
        RasterPass &pass;
        RasterSubPass &subPass;
        VertexType vertex;
    };

    struct ComputePassBuilder {
        ComputePassBuilder &AddComputeView(const char *name, const ComputeView &view);

        RenderGraph &graph;
        VertexType vertex;
    };

    struct CopyPassBuilder {
        CopyPassBuilder &AddCopyView(const CopyView &view);

        RenderGraph &graph;
        VertexType vertex;
    };


    using PassGraph = boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS>;
    using DependencyGraph = boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS>;

    struct ResourceGraph {
        explicit ResourceGraph(RenderGraphContext *ctx);
        ~ResourceGraph() = default;

        using Graph = boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS>;
        using vertex_descriptor = VertexType;
        using Tag   = std::variant<RootTag, ImageTag, ImageViewTag, ImportImageTag, BufferTag, ImportBufferTag, BufferViewTag>;

        void AddImage(const char *name, const GraphImage &image);
        void ImportImage(const char *name, const rhi::ImagePtr &image);
        void AddImageView(const char *name, const char *source, const GraphImageView &view);

        void AddBuffer(const char *name, const GraphBuffer &attachment);
        void ImportBuffer(const char *name, const rhi::BufferPtr &buffer);
        void AddBufferView(const char *name, const char *source, const GraphBufferView &view);

        // memory
        RenderGraphContext *context;

        // vertex
        VertexList vertices;

        // components
        PmrVector<PmrString> names;
        PmrVector<Tag>       tags;
        PmrVector<size_t>    polymorphicDatas;
        PmrVector<LifeTime>  lifeTimes;

        // resources
        PmrVector<ImageViewRes<GraphImage>>         images;
        PmrVector<ImageViewRes<GraphImportImage>>   importImages;
        PmrVector<ImageViewRes<GraphImageView>>     imageViews;
        PmrVector<BufferViewRes<GraphBuffer>>       buffers;
        PmrVector<BufferViewRes<GraphImportBuffer>> importBuffers;
        PmrVector<BufferViewRes<GraphBufferView>>   bufferViews;

        Graph graph;
    };

    struct RenderGraph {
        explicit RenderGraph(RenderGraphContext *ctx);
        ~RenderGraph() = default;

        using vertex_descriptor = VertexType;
        using Tag = std::variant<RootTag, RasterPassTag, RasterSubPassTag, ComputePassTag, CopyBlitTag, PresentTag, RefNodeTag>;

        RasterPassBuilder    AddRasterPass(const char *name, uint32_t width, uint32_t height);
        RasterSubPassBuilder AddRasterSubPass(const char *name, const char *pass);
        ComputePassBuilder   AddComputePass(const char *name);
        CopyPassBuilder      AddCopyPass(const char *name);
        void AddDependency(const char *name, VertexType passId, ResourceAccess access);

        // memory
        RenderGraphContext *context;

        // vertex
        VertexList vertices;

        // components
        PmrVector<PmrString> names;
        PmrVector<Tag>       tags;
        PmrVector<size_t>    polymorphicDatas;

        // passes
        PmrVector<RasterPass>    rasterPasses;
        PmrVector<RasterSubPass> subPasses;
        PmrVector<ComputePass>   computePasses;
        PmrVector<CopyBlitPass>  copyBlitPasses;
        PmrVector<PresentPass>   presentPasses;

        // refNode
        PmrVector<RefNode> referenceNodes;

        ResourceGraph   resourceGraph;
        PassGraph       passGraph;
        DependencyGraph dependencyGraph;
    };

    template <typename D>
    VertexType AddVertex(const char *name, D &&val, ResourceGraph &graph)
    {
        using Tag = typename std::remove_reference<D>::type::Tag;
        auto vertex = static_cast<VertexType>(graph.vertices.size());
        graph.vertices.emplace_back();
        graph.tags.emplace_back(Tag{});
        graph.names.emplace_back(PmrString(name, &graph.context->resources));
        graph.lifeTimes.emplace_back(LifeTime{});

        if constexpr (std::is_same_v<Tag, ImageTag>) {
            graph.polymorphicDatas.emplace_back(graph.images.size());
            graph.images.emplace_back(std::forward<D>(val));
        } else if constexpr (std::is_same_v<Tag, ImageViewTag>) {
            graph.polymorphicDatas.emplace_back(graph.imageViews.size());
            graph.imageViews.emplace_back(std::forward<D>(val));
        } else if constexpr (std::is_same_v<Tag, ImportImageTag>) {
            graph.polymorphicDatas.emplace_back(graph.importImages.size());
            graph.importImages.emplace_back(std::forward<D>(val));
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
        graph.vertices.emplace_back();
        graph.tags.emplace_back(Tag{});
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
        } else if constexpr (std::is_same_v<Tag, RefNodeTag>) {
            graph.polymorphicDatas.emplace_back(graph.referenceNodes.size());
            graph.referenceNodes.emplace_back(std::forward<D>(val));
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
    typename G::Tag &Tag(V v, G &g)
    {
        return g.tags[static_cast<typename G::vertex_descriptor>(v)];
    }

    template <typename V, typename G>
    size_t Index(V v, G &g)
    {
        return g.polymorphicDatas[static_cast<typename G::vertex_descriptor>(v)];
    }

    template <typename V, typename G>
    LifeTime &Life(V v, G &g)
    {
        return g.lifeTimes[static_cast<typename G::vertex_descriptor>(v)];
    }
} // namespace sky
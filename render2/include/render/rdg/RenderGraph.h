//
// Created by Zach Lee on 2023/3/27.
//

#pragma once

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/properties.hpp>
#include <boost/property_map/property_map.hpp>

#include <core/template/Overloaded.h>
#include <core/platform/Platform.h>
#include <rhi/Image.h>
#include <rhi/Buffer.h>
#include <render/rdg/RenderGraphContext.h>
#include <render/rdg/RenderGraphTypes.h>

namespace sky::rdg {
    struct RenderGraph;

    struct RasterPassBuilder {
        void AddRasterView(const RasterView &view);
        void AddComputeView(const ComputeView &view);

        RenderGraph &graph;
        VertexType vertex;
    };

    struct ComputePassBuilder {
        void AddComputeView(const ComputeView &view);

        RenderGraph &graph;
        VertexType vertex;
    };

    struct CopyPassBuilder {
        void AddCopyView(const CopyView &view);

        RenderGraph &graph;
        VertexType vertex;
    };

    using ResourceGraph = boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS>;

    struct RenderGraph {
    public:
        explicit RenderGraph(RenderGraphContext *ctx);
        ~RenderGraph() = default;

        using Tag = std::variant<RootTag,
                                 RasterPassTag,
                                 RasterSubPassTag,
                                 ComputePassTag,
                                 CopyBlitTag,
                                 PresentTag,
                                 ImageTag,
                                 ImageViewTag,
                                 ImportImageTag,
                                 BufferTag,
                                 ImportBufferTag,
                                 BufferViewTag>;

        void AddImage(const char *name, const GraphImage &image);
        void ImportImage(const char *name, const rhi::ImagePtr &image);
        void AddImageView(const char *name, const char *source, const GraphImageView &view);

        void AddBuffer(const char *name, const GraphBuffer &attachment);
        void ImportBuffer(const char *name, const rhi::BufferPtr &buffer);
        void AddBufferView(const char *name, const char *source, const GraphBufferView &view);

        VertexType FindVertex(const char *name);

        RasterPassBuilder  AddRasterPass(const char *name);
        ComputePassBuilder AddComputePass(const char *name);
        CopyPassBuilder    AddCopyPass(const char *name);

        // memory
        RenderGraphContext *context;

        // vertex
        VertexList vertices;

        // components
        PmrVector<PmrString> names;
        PmrVector<Tag>       tags;
        PmrVector<size_t>    polymorphicDatas;

        // resources
        PmrVector<ImageViewRes<GraphImage>>         images;
        PmrVector<ImageViewRes<GraphImportImage>>   importImages;
        PmrVector<ImageViewRes<GraphImageView>>     imageViews;
        PmrVector<BufferViewRes<GraphBuffer>>       buffers;
        PmrVector<BufferViewRes<GraphImportBuffer>> importBuffers;
        PmrVector<BufferViewRes<GraphBufferView>>   bufferViews;

        // passes
        PmrVector<RasterPass>    rasterPasses;
        PmrVector<RasterSubPass> subPasses;
        PmrVector<ComputePass>   computePasses;
        PmrVector<CopyBlitPass>  copyBlitPasses;
        PmrVector<PresentPass>   presentPasses;

        ResourceGraph resourceGraph;
//        DependencyGraph depGraph;
//        PassGraph       nodeGraph;
    };

    template <typename D>
    VertexType AddVertex(const char *name, const D &val, RenderGraph &graph)
    {
        using Tag = typename D::Tag;

        auto vertex = static_cast<VertexType>(graph.vertices.size());
        graph.vertices.emplace_back();
        graph.tags.emplace_back(Tag{});
        graph.names.emplace_back(PmrString(name, &graph.context->resources));

        if constexpr (std::is_same_v<Tag, ImageTag>) {
            graph.polymorphicDatas.emplace_back(graph.images.size());
            graph.images.emplace_back(val);
        } else if constexpr (std::is_same_v<Tag, ImageViewTag>) {
            graph.polymorphicDatas.emplace_back(graph.imageViews.size());
            graph.imageViews.emplace_back(val);
        } else if constexpr (std::is_same_v<Tag, ImportImageTag>) {
            graph.polymorphicDatas.emplace_back(graph.importImages.size());
            graph.importImages.emplace_back(val);
        } else if constexpr (std::is_same_v<Tag, BufferTag>) {
            graph.polymorphicDatas.emplace_back(graph.buffers.size());
            graph.buffers.emplace_back(val);
        } else if constexpr (std::is_same_v<Tag, ImportBufferTag>) {
            graph.polymorphicDatas.emplace_back(graph.importBuffers.size());
            graph.importBuffers.emplace_back(val);
        } else if constexpr (std::is_same_v<Tag, BufferViewTag>) {
            graph.polymorphicDatas.emplace_back(graph.bufferViews.size());
            graph.bufferViews.emplace_back(val);
        } else if constexpr (std::is_same_v<Tag, RasterPassTag>) {
            graph.polymorphicDatas.emplace_back(graph.rasterPasses.size());
            graph.rasterPasses.emplace_back(val);
        } else if constexpr (std::is_same_v<Tag, RasterSubPass>) {
            graph.polymorphicDatas.emplace_back(graph.subPasses.size());
            graph.subPasses.emplace_back(val);
        } else if constexpr (std::is_same_v<Tag, ComputePassTag>) {
            graph.polymorphicDatas.emplace_back(graph.computePasses.size());
            graph.computePasses.emplace_back(val);
        } else if constexpr (std::is_same_v<Tag, CopyBlitTag>) {
            graph.polymorphicDatas.emplace_back(graph.copyBlitPasses.size());
            graph.copyBlitPasses.emplace_back(val);
        } else if constexpr (std::is_same_v<Tag, PresentTag>) {
            graph.polymorphicDatas.emplace_back(graph.presentPasses.size());
            graph.presentPasses.emplace_back(val);
        } else {
            graph.polymorphicDatas.emplace_back(0);
        }

        return vertex;
    }
} // namespace sky
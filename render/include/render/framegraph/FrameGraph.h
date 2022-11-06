//
// Created by Zach Lee on 2022/6/2.
//

#pragma once

#include <memory>
#include <render/framegraph/FrameGraphAttachment.h>
#include <render/framegraph/FrameGraphBuilder.h>
#include <render/framegraph/FrameGraphPass.h>
#include <render/framegraph/FrameGraphResource.h>
#include <unordered_map>
#include <vector>

namespace sky {
    class RenderScene;

    class FrameGraph {
    public:
        FrameGraph()  = default;
        ~FrameGraph() = default;

        FrameGraph(const FrameGraph &)            = delete;
        FrameGraph &operator=(const FrameGraph &) = delete;

        template <typename Pass, typename Setup>
        Pass *AddPass(const std::string &name, Setup &&setup)
        {
            Pass             *pass = AddNode<Pass>(name);
            FrameGraphBuilder builder(*this, *pass);
            passes.emplace_back(pass);
            setup(builder);
            return pass;
        }

        struct Edge {
            FrameGraphNode *from;
            FrameGraphNode *to;
        };

        using FgNodePtr     = std::unique_ptr<FrameGraphNode>;
        using FgResourcePtr = std::unique_ptr<FrameGraphResource>;
        using FgImagePtr    = std::unique_ptr<FrameGraphImage>;
        using FgBufferPtr   = std::unique_ptr<FrameGraphBuffer>;

        void Compile();

        void Execute(vk::CommandBufferPtr commandBuffer);

        void PrintGraph();

    private:
        friend class FrameGraphBuilder;
        template <typename T, typename... Args>
        T *AddNode(Args &&...args)
        {
            nodes.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
            auto res = nodes.back().get();
            nodeTable.emplace(res->name, res);
            return static_cast<T *>(res);
        }

        template <typename T>
        T *AddImage(const std::string &name)
        {
            auto iter = images.emplace(name, std::make_unique<T>());
            return static_cast<T *>(iter.first->second.get());
        }

        template <typename T>
        T *AddBuffer(const std::string &name)
        {
            auto iter = buffers.emplace(name, std::make_unique<T>());
            return static_cast<T *>(iter.first->second.get());
        }

        void AddEdge(FrameGraphNode *from, FrameGraphNode *to)
        {
            edges.emplace_back(Edge{from, to});
        }

        std::vector<FgNodePtr>                            nodes;
        std::vector<Edge>                                 edges;
        std::vector<FrameGraphPass *>                     passes;
        std::unordered_map<std::string, FgImagePtr>       images;
        std::unordered_map<std::string, FgBufferPtr>      buffers;
        std::unordered_map<std::string, FrameGraphNode *> nodeTable;
    };

} // namespace sky

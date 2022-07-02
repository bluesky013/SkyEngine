//
// Created by Zach Lee on 2022/6/2.
//

#pragma once

#include <render/framegraph/FrameGraphAttachment.h>
#include <render/framegraph/FrameGraphPass.h>
#include <render/framegraph/FrameGraphResource.h>
#include <render/framegraph/FrameGraphBuilder.h>
#include <memory>
#include <vector>
#include <unordered_map>

namespace sky {

    class FrameGraph {
    public:
        FrameGraph() = default;
        ~FrameGraph() = default;

        FrameGraph(const FrameGraph&) = delete;
        FrameGraph& operator=(const FrameGraph&) = delete;

        template <typename Pass, typename Setup>
        void AddPass(const std::string& name, Setup&& setup)
        {
            Pass* pass = AddNode<Pass>(name);
            FrameGraphBuilder builder(*this, *pass);
            setup(builder);
        }

        struct Edge {
            FrameGraphNode* from;
            FrameGraphNode* to;
        };

        using FgNodePtr = std::unique_ptr<FrameGraphNode>;
        using FgResourcePtr = std::unique_ptr<FrameGraphResource>;

        void Compile();

        void Execute();

        void PrintGraph();

    private:
        friend class FrameGraphBuilder;
        template <typename T, typename ...Args>
        T* AddNode(Args&& ...args)
        {
            nodes.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
            auto res = nodes.back().get();
            return static_cast<T*>(res);
        }

        template <typename T>
        T* AddResource(const std::string& name)
        {
            auto iter = resources.emplace(name, std::make_unique<T>());
            return static_cast<T*>(iter.first->second.get());
        }

        void AddEdge(FrameGraphNode* from, FrameGraphNode* to)
        {
            edges.emplace_back(Edge{from, to});
        }

        std::vector<FgNodePtr> nodes;
        std::vector<Edge> edges;
        std::unordered_map<std::string, FgResourcePtr> resources;
        std::unordered_map<std::string, FrameGraphNode*> nodeTable;
    };

}
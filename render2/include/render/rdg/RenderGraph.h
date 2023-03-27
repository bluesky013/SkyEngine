//
// Created by Zach Lee on 2023/3/27.
//

#pragma once

#include<boost/graph/adjacency_list.hpp>
#include<boost/graph/properties.hpp>
#include<boost/property_map/property_map.hpp>

namespace sky {

    namespace impl {
        struct EdgeDescriptor {

        };
    }


    class RenderGraph {
    public:
        RenderGraph()  = default;
        ~RenderGraph() = default;

        // pass node dag
        using NodeGraph = boost::adjacency_list<boost::vecS,
                                                boost::vecS,
                                                boost::directedS>;

        void Compile();

    private:
        NodeGraph nodes;
    };

}
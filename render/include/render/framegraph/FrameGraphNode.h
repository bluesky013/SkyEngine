//
// Created by Zach Lee on 2022/6/2.
//

#pragma once

#include <string>

namespace sky {

    class FrameGraphNode {
    public:
        FrameGraphNode(const std::string& str) : name(str) {}
        virtual ~FrameGraphNode() = default;

    private:
        friend class FrameGraph;
        std::string name;
    };

}
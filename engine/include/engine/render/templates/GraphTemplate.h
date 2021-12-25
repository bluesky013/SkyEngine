//
// Created by Zach Lee on 2021/12/22.
//

#pragma once
#include <list>
#include <vector>
#include <string>
#include <unordered_map>

namespace sky {
    class GraphPass;
    class RenderGraph;

    struct ViewPassGroup {
        std::vector<GraphPass*> passes;
    };

    class GraphTemplate {
    public:
        GraphTemplate() = default;
        ~GraphTemplate() = default;

        void RegisterViewPassGroup(const std::string& name, ViewPassGroup& group);

        void ViewRequest(RenderGraph&, const std::string& tag, uint32_t num);

    private:
        std::unordered_map<std::string, ViewPassGroup> groups;
    };

}
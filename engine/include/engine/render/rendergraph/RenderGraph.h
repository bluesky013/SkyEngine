//
// Created by Zach Lee on 2021/12/22.
//

#pragma once
#include <engine/render/rendergraph/RenderGraphDatabase.h>

namespace sky {

    class RenderGraph {
    public:
        RenderGraph() = default;
        ~RenderGraph() = default;

        void Clear();

    private:
        RenderGraphDatabase database;
    };


}
//
// Created by Zach Lee on 2022/11/13.
//

#pragma once

#include <rhi/Core.h>

namespace sky::rhi {

    class GraphicsPipeline {
    public:
        GraphicsPipeline() = default;
        virtual ~GraphicsPipeline() = default;

        struct Descriptor {
            DepthStencil            depthStencil;
            MultiSample             multiSample;
            InputAssembly           inputAssembly;
            RasterState             rasterState;
            std::vector<BlendState> blendStates;
        };
    };

}
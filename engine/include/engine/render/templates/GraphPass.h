//
// Created by Zach Lee on 2021/12/22.
//

#pragma once

namespace sky {

    class GraphPass {
    public:
        GraphPass() = default;
        virtual ~GraphPass() = default;
    };

    class GraphicPass : public GraphPass {
    public:
        GraphicPass() = default;
        ~GraphicPass() = default;
    };

    class RasterPass : public GraphPass {
    public:
        RasterPass() = default;
        ~RasterPass() = default;
    };

    class ComputePass : public GraphPass {
    public:
        ComputePass() = default;
        ~ComputePass() = default;
    };

    class CopyPass : public GraphPass {
    public:
        CopyPass() = default;
        ~CopyPass() = default;
    };

    class SwapChainPass : public GraphPass {
    public:
        SwapChainPass() = default;
        ~SwapChainPass() = default;
    };
}
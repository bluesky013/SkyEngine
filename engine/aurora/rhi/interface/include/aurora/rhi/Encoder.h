//
// Created by Zach Lee on 2026/3/30.
//

#pragma once

namespace sky::aurora {

    class GraphicsEncoder {
    public:
        GraphicsEncoder() = default;
        virtual ~GraphicsEncoder() = default;
    };

    class ComputeEncoder {
    public:
        ComputeEncoder() = default;
        virtual ~ComputeEncoder() = default;
    };

    class BlitEncoder {
    public:
        BlitEncoder() = default;
        virtual ~BlitEncoder() = default;
    };

} // namespace sky::aurora
//
// Created by blues on 2026/3/29.
//

#pragma once

namespace sky::aurora {

    class Device;

    class RenderGraph {
    public:
        explicit RenderGraph(Device* dev) : mDevice(dev) {}
        ~RenderGraph() = default;

    private:
        Device* mDevice = nullptr;
    };

} // namespace sky::aurora

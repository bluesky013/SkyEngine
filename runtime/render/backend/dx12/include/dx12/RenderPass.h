//
// Created by blues on 2024/2/11.
//

#pragma once

#include <rhi/RenderPass.h>
#include <dx12/DevObject.h>

namespace sky::dx {
    class Device;

    class RenderPass : public rhi::RenderPass, public DevObject {
    public:
        explicit RenderPass(Device &dev) : DevObject(dev) {}

        const std::vector<DXGI_FORMAT> &GetColorFormats() const { return colorFormats; }
        const DXGI_FORMAT &GetDSFormat() const { return depthStencilFormat; }
    private:
        bool Init(const Descriptor &desc);

        std::vector<DXGI_FORMAT> colorFormats;
        DXGI_FORMAT depthStencilFormat = DXGI_FORMAT_UNKNOWN;
    };

} // namespace sky::dx
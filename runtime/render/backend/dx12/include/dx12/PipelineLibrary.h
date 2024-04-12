//
// Created by blues on 2023/10/13.
//

#pragma once

#include <rhi/PipelineLibrary.h>
#include <dx12/DevObject.h>

namespace sky::dx {
    class Device;

    class PipelineLibrary : public rhi::PipelineLibrary, public DevObject {
    public:
        explicit PipelineLibrary(Device &dev) : DevObject(dev) {}
        ~PipelineLibrary() override = default;

    private:
        friend class Device;
        bool Init(const Descriptor &desc);

        ComPtr<ID3D12PipelineLibrary> library;
    };

} // namespace sky::dx
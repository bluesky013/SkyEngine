//
// Created by Zach Lee on 2023/5/30.
//

#pragma once


#include <RHISampleBase.h>

namespace sky::rhi {

    class RHIDrawIndirectSample : public RHISampleBase {
    public:
        RHIDrawIndirectSample() = default;
        ~RHIDrawIndirectSample() = default;

        void SetupBase() override;
        void OnTick(float delta) override;
        void OnStop() override;
    private:
        void SetFeature() override;

        rhi::VertexInputPtr vi;
        rhi::VertexAssemblyPtr va;
        rhi::BufferViewPtr instanceBuffer;
        rhi::BufferPtr indirectBuffer;
    };

} // namespace sky::rhi
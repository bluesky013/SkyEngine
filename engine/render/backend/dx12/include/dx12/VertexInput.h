//
// Created by blues on 2024/2/12.
//

#pragma once

#include <d3d12.h>
#include <rhi/VertexInput.h>

namespace sky::dx {

    class VertexInput : public rhi::VertexInput {
    public:
        VertexInput() : vertexDesc{} {}
        ~VertexInput() override = default;

        const D3D12_INPUT_LAYOUT_DESC &GetVertexDesc() { return vertexDesc; }

    private:
        bool Init(const Descriptor &desc);

        std::vector<D3D12_INPUT_ELEMENT_DESC> storage;
        D3D12_INPUT_LAYOUT_DESC vertexDesc;
    };
    using VertexInputPtr = std::shared_ptr<VertexInput>;

} // namespace sky::dx

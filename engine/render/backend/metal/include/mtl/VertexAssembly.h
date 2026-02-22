//
// Created by Zach Lee on 2023/5/28.
//

#pragma once

#include <rhi/VertexAssembly.h>
#include <mtl/VertexInput.h>
#import <Metal/MTLRenderCommandEncoder.h>

namespace sky::mtl {
    class Device;

    class VertexAssembly : public rhi::VertexAssembly {
    public:
        VertexAssembly() = default;
        ~VertexAssembly() = default;

        void OnBind(id<MTLRenderCommandEncoder> encoder);

    private:
        friend class Device;
        bool Init(const Descriptor &desc);

        std::vector<rhi::BufferView> vertexBuffers;
        VertexInputPtr vertexInput;

        std::vector<id<MTLBuffer>> mtlVertexBuffers;
        std::vector<NSUInteger> vertexOffsets;
        NSRange range;
    };
    using VertexAssemblyPtr = std::shared_ptr<VertexAssembly>;

} // namespace sky::mtl

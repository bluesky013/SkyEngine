//
// Created by Zach Lee on 2023/5/28.
//

#pragma once

#include <rhi/VertexAssembly.h>
#include <mtl/BufferView.h>
#include <mtl/VertexInput.h>
#import <Metal/MTLRenderCommandEncoder.h>

namespace sky::mtl {
    class Device;

    class VertexAssembly : public rhi::VertexAssembly {
    public:
        VertexAssembly() = default;
        ~VertexAssembly() = default;

        void OnBind(id<MTLRenderCommandEncoder> encoder);
        id<MTLBuffer> GetIndexBuffer() const { return mtlIndexBuffer; }
        MTLIndexType GetIndexType() const { return indexType; }
        NSUInteger GetIndexBufferOffset() const { return indexBuffer->GetViewDesc().offset; }

    private:
        friend class Device;
        bool Init(const Descriptor &desc);

        std::vector<BufferViewPtr> vertexBuffers;
        BufferViewPtr indexBuffer;
        VertexInputPtr verxInput;

        std::vector<id<MTLBuffer>> mtlVertexBuffers;
        id<MTLBuffer> mtlIndexBuffer;
        std::vector<NSUInteger> vertexOffsets;
        MTLIndexType indexType = MTLIndexTypeUInt32;
        NSRange range;
    };
    using VertexAssemblyPtr = std::shared_ptr<VertexAssembly>;

} // namespace sky::mtl

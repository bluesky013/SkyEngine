//
// Created by Zach Lee on 2023/5/28.
//

#pragma once

#include <rhi/VertexInput.h>
#import <Metal/MTLVertexDescriptor.h>

namespace sky::mtl {

    class VertexInput : public rhi::VertexInput {
    public:
        VertexInput() = default;
        ~VertexInput() noexcept;

        bool Init(const Descriptor &desc);

        MTLVertexDescriptor *GetNativeDescriptor() const { return descriptor; }

    private:
        MTLVertexDescriptor* descriptor = nil;
    };
    using VertexInputPtr = std::shared_ptr<VertexInput>;

} // namespace sky::mtl

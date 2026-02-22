//
// Created by Zach Lee on 2023/5/28.
//

#include <mtl/VertexInput.h>
#include <mtl/Conversion.h>

namespace sky::mtl {

    VertexInput::~VertexInput() noexcept
    {
        if (descriptor) {
            [descriptor release];
            descriptor = nil;
        }
    }

    bool VertexInput::Init(const Descriptor &desc)
    {
        descriptor = [[MTLVertexDescriptor alloc] init];
        for (uint32_t i = 0; i < desc.attributesNum; ++i) {
            const auto &attribute = desc.attributes[i];

            descriptor.attributes[attribute.location].format = FromRHI(attribute.format);
            descriptor.attributes[attribute.location].offset = attribute.offset;
            descriptor.attributes[attribute.location].bufferIndex = attribute.binding;
        }

        for (uint32_t i = 0; i < desc.bindingsNum; ++i) {
            const auto &binding          = desc.bindings[i];
            descriptor.layouts[i].stride = binding.stride;
            descriptor.layouts[i].stepFunction =
                binding.inputRate == rhi::VertexInputRate::PER_INSTANCE ? MTLVertexStepFunctionPerInstance : MTLVertexStepFunctionPerVertex;
            descriptor.layouts[i].stepRate = 1;
        }
        return descriptor != nil;
    }

} // namespace sky::mtl

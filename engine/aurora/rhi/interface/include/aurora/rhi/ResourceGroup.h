//
// Created by Zach Lee on 2026/3/30.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <aurora/rhi/Resource.h>
#include <aurora/rhi/Core.h>
#include <aurora/rhi/DescriptorEncoder.h>
#include <memory>

namespace sky::aurora {

    class Shader;

    class ResourceGroup
        : public RefObject
        , public IDelayReleaseResource {
    public:
        struct Descriptor {
            Shader   *shader = nullptr; // binding layout derived from shader reflection
            uint32_t  set    = 0;       // descriptor set index
        };

        ResourceGroup() = default;
        ~ResourceGroup() override = default;

        virtual std::unique_ptr<DescriptorEncoder> CreateEncoder() = 0;
    };

    using ResourceGroupPtr = CounterPtr<ResourceGroup>;

} // namespace sky::aurora

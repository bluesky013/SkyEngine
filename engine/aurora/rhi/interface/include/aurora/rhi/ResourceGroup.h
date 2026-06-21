//
// Created by Zach Lee on 2026/3/30.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <aurora/rhi/Resource.h>
#include <aurora/rhi/Core.h>
#include <vector>

namespace sky::aurora {

    class Buffer;
    class Image;
    class Sampler;

    enum class ResourceWriteKind : uint32_t {
        BUFFER                 = 0,
        IMAGE                  = 1,
        SAMPLER                = 2,
        COMBINED_IMAGE_SAMPLER = 3,
    };

    struct ResourceUpdateInfo {
        uint32_t          binding      = 0;
        uint32_t          arrayElement = 0;
        ResourceWriteKind kind         = ResourceWriteKind::BUFFER;

        // BUFFER
        Buffer  *buffer       = nullptr;
        uint64_t bufferOffset = 0;
        uint64_t bufferRange  = 0;

        // IMAGE / COMBINED_IMAGE_SAMPLER
        Image      *image       = nullptr;
        ImageLayout imageLayout = ImageLayout::SHADER_READ_ONLY;

        // SAMPLER / COMBINED_IMAGE_SAMPLER
        Sampler *sampler = nullptr;
    };

    class ResourceGroupLayout : public RefObject {
    public:
        struct BindingDesc {
            uint32_t               binding = 0;
            DescriptorType         type    = DescriptorType::UNIFORM_BUFFER;
            uint32_t               count   = 1;
            ShaderStageFlags       stages;
            DescriptorBindingFlags flags;
        };

        struct Descriptor {
            std::vector<BindingDesc> bindings;
        };

        ResourceGroupLayout() = default;
        ~ResourceGroupLayout() override = default;
    };

    using ResourceGroupLayoutPtr = CounterPtr<ResourceGroupLayout>;

    class ResourceGroup
        : public RefObject
        , public IDelayReleaseResource {
    public:
        struct Descriptor {
            ResourceGroupLayout *layout = nullptr;
        };

        ResourceGroup() = default;
        ~ResourceGroup() override = default;

        virtual void Update(const std::vector<ResourceUpdateInfo> &writes) = 0;
    };

    using ResourceGroupPtr = CounterPtr<ResourceGroup>;

} // namespace sky::aurora

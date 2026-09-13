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
    class Shader;

    enum class ResourceWriteKind : uint32_t {
        BUFFER  = 0,
        IMAGE   = 1,
        SAMPLER = 2,
    };

    struct ResourceUpdateInfo {
        uint32_t          binding      = 0;
        uint32_t          arrayElement = 0;
        ResourceWriteKind kind         = ResourceWriteKind::BUFFER;

        // BUFFER
        Buffer  *buffer       = nullptr;
        uint64_t bufferOffset = 0;
        uint64_t bufferRange  = 0;

        // IMAGE
        Image      *image       = nullptr;
        ImageLayout imageLayout = ImageLayout::SHADER_READ_ONLY;

        // SAMPLER
        Sampler *sampler = nullptr;
    };

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

        virtual void Update(const std::vector<ResourceUpdateInfo> &writes) = 0;
    };

    using ResourceGroupPtr = CounterPtr<ResourceGroup>;

} // namespace sky::aurora

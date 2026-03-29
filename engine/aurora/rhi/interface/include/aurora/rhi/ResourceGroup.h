//
// Created by Zach Lee on 2026/3/30.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <aurora/rhi/Resource.h>

namespace sky::aurora {

    class ResourceGroupLayout : public RefObject {
    public:
        struct Descriptor {
        };

        ResourceGroupLayout() = default;
        ~ResourceGroupLayout() override = default;
    };

    class ResourceGroup
        : public RefObject
        , public IDelayReleaseResource {
    public:
        struct Descriptor {
        };

        ResourceGroup() = default;
        ~ResourceGroup() override = default;
    };

} // namespace sky::aurora
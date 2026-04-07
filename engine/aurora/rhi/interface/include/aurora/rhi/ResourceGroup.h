//
// Created by Zach Lee on 2026/3/30.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <core/name/Name.h>
#include <aurora/rhi/Resource.h>
#include <aurora/rhi/Core.h>

namespace sky::aurora {

    class ResourceGroupLayout : public RefObject {
    public:
        ResourceGroupLayout() = default;
        ~ResourceGroupLayout() override = default;

    private:
        std::unordered_map<Name, BindingHandler> handlers;          // name -> [binding, size]
        std::unordered_map<Name, BufferNameHandler> bufferHandlers; // name -> constant buffer name handler
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
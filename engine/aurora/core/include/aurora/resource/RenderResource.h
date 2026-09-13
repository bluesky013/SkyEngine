//
// Created on 2026/09/13.
//

#pragma once

#include <aurora/rhi/Device.h>
#include <core/name/Name.h>

namespace sky::aurora {

    // Common base for all high-level render resources (buffers and, in the
    // future, images). Wraps a lazily-created underlying rhi resource and
    // exposes a unified upload entry point. Resource/memory statistics are a
    // backend (VMA/D3D12MA) concern, not tracked here.
    class RenderResource {
    public:
        RenderResource() = default;
        explicit RenderResource(const Name &inName) : name(inName) {}
        virtual ~RenderResource() = default;

        const Name &GetName() const
        {
            return name;
        }
        Device *GetDevice() const
        {
            return device;
        }
        bool IsCreated() const
        {
            return created;
        }

        virtual bool Upload(const void *data, uint64_t size, uint64_t offset = 0) = 0;

    protected:
        Device *device  = nullptr;
        Name    name;
        bool    created = false;

        // Lazy creation: derived classes create the underlying rhi resource on
        // first use (deferred until actually needed).
        virtual void Create()  = 0;
        virtual void Release() = 0;
    };

} // namespace sky::aurora

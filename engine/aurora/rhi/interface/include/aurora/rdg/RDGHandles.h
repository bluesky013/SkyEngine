//
// Aurora RDG resource handles.
//

#pragma once

#include <aurora/rhi/Core.h>
#include <functional>

namespace sky::aurora {

    struct RDGTextureHandle {
        uint32_t id = INVALID_INDEX;

        bool IsValid() const { return id != INVALID_INDEX; }
        bool operator==(const RDGTextureHandle &o) const { return id == o.id; }
        bool operator!=(const RDGTextureHandle &o) const { return id != o.id; }
    };

    struct RDGBufferHandle {
        uint32_t id = INVALID_INDEX;

        bool IsValid() const { return id != INVALID_INDEX; }
        bool operator==(const RDGBufferHandle &o) const { return id == o.id; }
        bool operator!=(const RDGBufferHandle &o) const { return id != o.id; }
    };

} // namespace sky::aurora

namespace std {
    template <>
    struct hash<sky::aurora::RDGTextureHandle> {
        size_t operator()(const sky::aurora::RDGTextureHandle &h) const noexcept
        {
            return std::hash<uint32_t>()(h.id);
        }
    };

    template <>
    struct hash<sky::aurora::RDGBufferHandle> {
        size_t operator()(const sky::aurora::RDGBufferHandle &h) const noexcept
        {
            return std::hash<uint32_t>()(h.id);
        }
    };
} // namespace std

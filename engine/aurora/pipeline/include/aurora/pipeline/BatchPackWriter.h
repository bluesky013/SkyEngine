//
// Created on 2026/09/14.
//

#pragma once

#include <aurora/rdg/BatchAllocator.h>
#include <aurora/rhi/Shader.h>

#include <cstdint>
#include <type_traits>

namespace sky::aurora {

    class BatchPackWriter {
    public:
        explicit BatchPackWriter(BatchAllocator &alloc) : mAlloc(alloc)
        {
        }

        template <typename T>
        uint32_t Pack(Shader &shader, ShaderUniformSlot<T> slot, const T &data, std::string &error)
        {
            static_assert(std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>);
            const auto             plan = shader.GetUniformPlan(slot.set, slot.binding, T::Schema(), error);
            std::vector<std::byte> bytes;
            if (!plan || !EncodeShaderUniform(*plan, std::as_bytes(std::span{&data, 1}), bytes, error))
                return UINT32_MAX;
            const auto offset = mAlloc.Allocate(static_cast<uint32_t>(bytes.size()));
            if (offset == UINT32_MAX) {
                error = "Batch uniform allocation exhausted";
                return offset;
            }
            mAlloc.Write(offset, bytes.data(), static_cast<uint32_t>(bytes.size()));
            return offset;
        }

    private:
        BatchAllocator &mAlloc;
    };

} // namespace sky::aurora

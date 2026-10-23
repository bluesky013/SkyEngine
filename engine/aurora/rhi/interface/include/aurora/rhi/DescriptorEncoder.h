//
// Created on 2026/09/14.
//

#pragma once

#include <aurora/rhi/Core.h>
#include <aurora/rhi/ShaderParameters.h>

#include <cstdint>

namespace sky::aurora {

    class Buffer;
    class Image;
    class Sampler;
    class ResourceGroup;
    // Name writes publish a new immutable group through GetResult() only after End succeeds.
    // Numeric writes are backend execution primitives and cannot mutate a published snapshot.
    class DescriptorEncoder {
    public:
        virtual ~DescriptorEncoder() = default;
        virtual ShaderParameterWriteResult WriteData(Name name, const ShaderDataValue &value);
        virtual ShaderParameterWriteResult WriteData(ShaderParameterHandle handle, const ShaderDataValue &value);
        template <class T>
        ShaderParameterWriteResult WriteData(Name name, const T &value)
        {
            return WriteData(name, MakeShaderData(value));
        }
        virtual ShaderParameterWriteResult WriteBlock(Name name, const ShaderCpuSchema &schema, std::span<const std::byte> bytes);
        virtual ShaderParameterWriteResult WriteBuffer(Name name, Buffer *buffer, uint64_t offset, uint64_t range, uint32_t arrayElement = 0);
        virtual ShaderParameterWriteResult WriteImage(Name name, Image *image, ImageLayout layout, uint32_t arrayElement = 0);
        virtual ShaderParameterWriteResult WriteSampler(Name name, Sampler *sampler, uint32_t arrayElement = 0);
        virtual const std::string         &GetError() const;
        virtual CounterPtr<ResourceGroup>  GetResult() const;
        virtual void                       Cancel();

        virtual bool WriteBuffer(uint32_t binding, Buffer *buffer, uint64_t offset, uint64_t range, uint32_t arrayElement = 0) = 0;
        virtual bool WriteImage(uint32_t binding, Image *image, ImageLayout layout, uint32_t arrayElement = 0)                 = 0;
        virtual bool WriteSampler(uint32_t binding, Sampler *sampler, uint32_t arrayElement = 0)                               = 0;
        virtual bool End()                                                                                                     = 0;
    };

} // namespace sky::aurora

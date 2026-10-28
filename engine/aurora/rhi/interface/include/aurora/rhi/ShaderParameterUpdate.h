//
// Created on 2026/10/27.
//

#pragma once

#include <aurora/rhi/Core.h>
#include <aurora/rhi/ShaderParameters.h>
#include <core/template/ReferenceObject.h>

namespace sky::aurora {

    class Buffer;
    class Image;
    class Sampler;
    class ResourceGroup;

    // A successful End publishes a new immutable group; failed updates leave the source unchanged.
    class ShaderParameterUpdate {
    public:
        virtual ~ShaderParameterUpdate()                                                                         = default;
        virtual ShaderParameterWriteResult WriteData(Name name, const ShaderDataValue &value)                    = 0;
        virtual ShaderParameterWriteResult WriteData(ShaderParameterHandle handle, const ShaderDataValue &value) = 0;
        template <class T>
        ShaderParameterWriteResult WriteData(Name name, const T &value)
        {
            return WriteData(name, MakeShaderData(value));
        }
        virtual ShaderParameterWriteResult WriteBlock(Name name, const ShaderCpuSchema &schema, std::span<const std::byte> bytes)             = 0;
        virtual ShaderParameterWriteResult WriteBuffer(Name name, Buffer *buffer, uint64_t offset, uint64_t range, uint32_t arrayElement = 0) = 0;
        virtual ShaderParameterWriteResult WriteImage(Name name, Image *image, ImageLayout layout, uint32_t arrayElement = 0)                 = 0;
        virtual ShaderParameterWriteResult WriteSampler(Name name, Sampler *sampler, uint32_t arrayElement = 0)                               = 0;
        virtual bool                       End()                                                                                              = 0;
        virtual const std::string         &GetError() const                                                                                   = 0;
        virtual CounterPtr<ResourceGroup>  GetResult() const                                                                                  = 0;
        virtual void                       Cancel()                                                                                           = 0;
    };

} // namespace sky::aurora

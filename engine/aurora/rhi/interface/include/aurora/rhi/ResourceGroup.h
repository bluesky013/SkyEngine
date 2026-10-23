//
// Created by Zach Lee on 2026/3/30.
//

#pragma once

#include <aurora/rhi/Buffer.h>
#include <aurora/rhi/Core.h>
#include <aurora/rhi/DescriptorEncoder.h>
#include <aurora/rhi/Image.h>
#include <aurora/rhi/Resource.h>
#include <aurora/rhi/Sampler.h>
#include <aurora/rhi/Shader.h>
#include <core/template/ReferenceObject.h>
#include <memory>

namespace sky::aurora {

    class Shader;
    class Device;
    class NamedDescriptorEncoder;
    struct ShaderUniformAllocation {
        BufferPtr                   buffer;
        uint64_t                    offset = 0, range = 0;
        std::shared_ptr<const void> retention;
    };
    class ShaderParameterUpdateContext {
    public:
        virtual ~ShaderParameterUpdateContext()    = default;
        virtual Device *GetDevice() const noexcept = 0;
        virtual bool    IsValid() const noexcept   = 0;
        virtual bool    UploadUniform(std::span<const std::byte> bytes, uint32_t alignment, ShaderUniformAllocation &out, std::string &error) = 0;
    };
    struct ShaderParameterBufferValue {
        BufferPtr buffer;
        uint64_t  offset = 0, range = 0;
    };
    struct ShaderParameterImageValue {
        ImagePtr    image;
        ImageLayout layout = ImageLayout::UNDEFINED;
    };
    struct ShaderParameterState {
        std::map<uint32_t, std::map<std::string, ShaderDataValue>>          fields;
        std::map<std::pair<uint32_t, uint32_t>, ShaderParameterBufferValue> buffers;
        std::map<std::pair<uint32_t, uint32_t>, ShaderParameterImageValue>  images;
        std::map<std::pair<uint32_t, uint32_t>, SamplerPtr>                 samplers;
        std::vector<ShaderUniformAllocation>                                uniforms;
    };

    class ResourceGroup : public RefObject, public IDelayReleaseResource {
    public:
        struct Descriptor {
            Shader  *shader = nullptr; // binding layout derived from shader reflection
            uint32_t set    = 0;       // descriptor set index
        };

        ResourceGroup()           = default;
        ~ResourceGroup() override = default;

        virtual const Device *GetDevice() const noexcept
        {
            return nullptr;
        }

        virtual std::unique_ptr<DescriptorEncoder> CreateEncoder() = 0;
        std::unique_ptr<DescriptorEncoder>         CreateEncoder(ShaderParameterUpdateContext &context) const;
        Shader                                    *GetShader() const noexcept
        {
            return parameterShader.Get();
        }
        uint32_t GetSet() const noexcept
        {
            return parameterSet;
        }
        bool IsParameterSnapshot() const noexcept
        {
            return parameterSnapshot;
        }

    protected:
        bool InitParameterLayout(const Descriptor &desc);

    private:
        friend class NamedDescriptorEncoder;
        ShaderPtr                                   parameterShader;
        uint32_t                                    parameterSet = 0;
        std::shared_ptr<const ShaderParameterState> parameterState;
        bool                                        parameterSnapshot = false;
    };

    using ResourceGroupPtr = CounterPtr<ResourceGroup>;

} // namespace sky::aurora

#pragma once

#include <aurora/rdg/FrozenFrameGraph.h>

namespace sky::aurora {
    class FrameGraphBuildContext;
    // Borrowed shared schema and graph handles are valid only during this frame's composition.
    class PassResourceBindings {
    public:
        explicit PassResourceBindings(const ShaderReflection &schema);
        PassResourceBindings(ShaderReflection &&) = delete;
        FrameOperationResult UniformData(ShaderResourceSlot<ShaderResourceType::UNIFORM_BUFFER> slot,
                                         const ShaderCpuSchema                                 &cpu,
                                         std::span<const std::byte>                             bytes,
                                         ShaderResourceType                                     type = ShaderResourceType::UNIFORM_BUFFER);
        template <class T>
        FrameOperationResult UniformData(ShaderUniformSlot<T> slot, const T &value, ShaderResourceType type = ShaderResourceType::UNIFORM_BUFFER)
        {
            static_assert(std::is_standard_layout_v<T> && std::is_trivially_copyable_v<T>);
            return UniformData(slot, T::Schema(), std::as_bytes(std::span{&value, 1}), type);
        }
        FrameOperationResult Data(Name name, const ShaderDataValue &value);
        FrameOperationResult Image(Name name, GraphImageHandle image, RasterImageView view = {});
        FrameOperationResult Sampler(Name name, SamplerPtr sampler);
        FrameOperationResult Uniform(ShaderResourceSlot<ShaderResourceType::UNIFORM_BUFFER> slot,
                                     RasterBufferRange                                      range,
                                     ShaderResourceType                                     type          = ShaderResourceType::UNIFORM_BUFFER,
                                     uint32_t                                               dynamicOffset = 0);
        FrameOperationResult Image(ShaderResourceSlot<ShaderResourceType::SAMPLED_IMAGE> slot, GraphImageHandle image, RasterImageView view = {});
        FrameOperationResult Sampler(ShaderResourceSlot<ShaderResourceType::SAMPLER> slot, SamplerPtr sampler);
        // Validate all groups before replacing the draw's bindings; unused declarations need no value.
        static FrameOperationResult Apply(RasterDraw &draw, std::span<const PassResourceBindings *const> groups);
        static FrameOperationResult Apply(FrameGraphBuildContext &context, RasterDraw &draw, std::span<const PassResourceBindings *const> groups);

    private:
        struct UniformValue {
            RasterUniformBinding binding;
            ShaderResourceType   type;
        };
        struct CpuUniformValue {
            uint32_t               set = 0, binding = 0;
            ShaderResourceType     type = ShaderResourceType::UNIFORM_BUFFER;
            ShaderCpuSchema        schema;
            std::vector<std::byte> bytes;
        };
        struct NamedImageValue {
            Name             name;
            GraphImageHandle image;
            RasterImageView  view;
        };
        struct NamedSamplerValue {
            Name       name;
            SamplerPtr sampler;
        };
        static FrameOperationResult       ApplyTransaction(FrameGraphBuildContext &, RasterDraw &, std::span<const PassResourceBindings *const>);
        static FrameOperationResult       ApplyImpl(FrameGraphBuildContext *, RasterDraw &, std::span<const PassResourceBindings *const>);
        FrameOperationResult              Fail(FrameError error) const;
        std::optional<FrameError>         Check(uint32_t set, uint32_t binding, ShaderResourceType type) const;
        const ShaderReflection           &schema;
        std::vector<UniformValue>         uniforms;
        std::vector<RasterImageBinding>   images;
        std::vector<RasterSamplerBinding> samplers;
        std::vector<CpuUniformValue>      cpuUniforms;
        std::vector<std::pair<Name, ShaderDataValue>> data;
        std::vector<NamedImageValue>                  namedImages;
        std::vector<NamedSamplerValue>                namedSamplers;
        mutable std::optional<FrameError>             failure;
    };
} // namespace sky::aurora

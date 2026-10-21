#pragma once

#include <aurora/rdg/FrozenFrameGraph.h>

namespace sky::aurora {
    // Borrowed shared schema and graph handles are valid only during this frame's composition.
    class PassResourceBindings {
    public:
        explicit PassResourceBindings(const ShaderReflection &schema);
        PassResourceBindings(ShaderReflection &&) = delete;
        FrameOperationResult Uniform(ShaderResourceSlot<ShaderResourceType::UNIFORM_BUFFER> slot,
                                     RasterBufferRange                                      range,
                                     ShaderResourceType                                     type          = ShaderResourceType::UNIFORM_BUFFER,
                                     uint32_t                                               dynamicOffset = 0);
        FrameOperationResult Image(ShaderResourceSlot<ShaderResourceType::SAMPLED_IMAGE> slot, GraphImageHandle image, RasterImageView view = {});
        FrameOperationResult Sampler(ShaderResourceSlot<ShaderResourceType::SAMPLER> slot, SamplerPtr sampler);
        // Validate all groups before replacing the draw's bindings; unused declarations need no value.
        static FrameOperationResult Apply(RasterDraw &draw, std::span<const PassResourceBindings *const> groups);

    private:
        struct UniformValue {
            RasterUniformBinding binding;
            ShaderResourceType   type;
        };
        std::optional<FrameError>         Check(uint32_t set, uint32_t binding, ShaderResourceType type) const;
        const ShaderReflection           &schema;
        std::vector<UniformValue>         uniforms;
        std::vector<RasterImageBinding>   images;
        std::vector<RasterSamplerBinding> samplers;
    };
} // namespace sky::aurora

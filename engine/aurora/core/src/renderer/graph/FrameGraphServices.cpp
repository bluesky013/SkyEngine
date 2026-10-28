//
// Created on 2026/10/27.
//

#include <aurora/renderer/IRenderFeature.h>

#include <algorithm>

namespace sky::aurora {
    std::optional<FrameError> FrameGraphServices::ValidateParameterResources(ShaderPtr                              owner,
                                                                             uint32_t                               set,
                                                                             const ShaderParameterTransactionState &state,
                                                                             const RasterBindingDraft              &resources)
    {
        const auto invalid = [](const char *message) { return FrameError{FrameErrorCode::INVALID_ARGUMENT, message}; };
        if (!owner || set >= MAX_SHADER_SETS)
            return invalid("Invalid parameter group set");
        const auto &shader = *owner;
        std::string error;
        if (!ValidateShaderParameterState(shader, set, state, error))
            return FrameError{FrameErrorCode::INVALID_ARGUMENT, std::move(error)};
        if (resources.uniforms.size() > 32 || resources.storage.size() > 32 || resources.images.size() > 32 || resources.samplers.size() > 32)
            return invalid("Parameter resource count exceeds the group binding limit");
        ShaderReflection target;
        target.pushConstants = shader.GetReflection().pushConstants;
        for (const auto &resource : shader.GetReflection().resources)
            if (resource.set == set) {
                if (!IsRasterResourceSupported(resource))
                    return FrameError{FrameErrorCode::UNSUPPORTED, "Unsupported parameter resource shape"};
                if (state.resources.contains({resource.binding, 0}))
                    target.resources.push_back(resource);
            }
        for (const auto &block : shader.GetReflection().blocks)
            if (block.set == set) {
                if (!block.size || block.size > 65536)
                    return invalid("Parameter block exceeds the supported uniform range");
                if (state.resources.contains({block.binding, 0}))
                    target.blocks.push_back(block);
            }
        const auto wrongSet = [&](const auto &values) { return std::ranges::any_of(values, [&](const auto &value) { return value.set != set; }); };
        if (wrongSet(resources.uniforms) || wrongSet(resources.storage) || wrongSet(resources.images) || wrongSet(resources.samplers))
            return invalid("Parameter resources contain another logical set");
        const auto validBuffer = [](const RasterUniformBinding &value) {
            return value.range.buffer.graph && value.range.buffer.index != UINT32_MAX && value.range.size &&
                   value.range.offset <= UINT64_MAX - value.range.size && value.dynamicOffset <= UINT64_MAX - value.range.offset - value.range.size;
        };
        if (!std::ranges::all_of(resources.uniforms, validBuffer) || !std::ranges::all_of(resources.storage, validBuffer) ||
            std::ranges::any_of(resources.images, [](const auto &value) { return !value.image.graph || value.image.index == UINT32_MAX; }) ||
            std::ranges::any_of(resources.samplers, [&](const auto &value) {
                return !value.sampler || (shader.GetDevice() && value.sampler->GetDevice() != shader.GetDevice());
            }))
            return invalid("Parameter resources contain an invalid range, handle or sampler owner");
        RasterDraw candidate;
        candidate.shader = std::move(owner);
        const auto bound = SetRasterBindings(candidate, resources);
        if (const auto *error = std::get_if<FrameError>(&bound))
            return *error;
        if (auto error = ValidateRasterBindings(candidate, target)) {
            if (error->code == FrameErrorCode::COMPILE_FAILED)
                error->code = FrameErrorCode::INVALID_ARGUMENT;
            return error;
        }
        return {};
    }

    std::variant<FrameError, FrameBindingGroupUse> FrameGraphServices::MaterializeParameters(ShaderPtr                              shader,
                                                                                             uint32_t                               set,
                                                                                             const ShaderParameterTransactionState &state,
                                                                                             RasterBindingDraft                     resources)
    {
        if (!shader || set >= MAX_SHADER_SETS)
            return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Parameter group requires a shader and valid set"};
        if (auto error = ValidateParameterResources(shader, set, state, resources))
            return *error;
        ShaderParameterTransaction                   update(*shader, set, state);
        std::shared_ptr<const ShaderParameterPacket> packet;
        if (!update.Finalize(packet))
            return FrameError{FrameErrorCode::INVALID_ARGUMENT, update.GetError()};
        for (const auto &uniform : packet->uniforms) {
            auto allocated = WriteUniform(set, uniform.binding, uniform.bytes, uniform.type);
            if (const auto *error = std::get_if<FrameError>(&allocated))
                return *error;
            resources.uniforms.push_back(std::get<RasterUniformBinding>(allocated));
        }
        RasterDraw staged;
        staged.shader = std::move(shader);
        auto bound    = SetRasterBindings(staged, std::move(resources));
        if (const auto *error = std::get_if<FrameError>(&bound))
            return *error;
        return std::move(staged.groups[set]);
    }
} // namespace sky::aurora

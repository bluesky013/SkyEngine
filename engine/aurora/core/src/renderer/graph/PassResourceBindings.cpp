#include <aurora/renderer/PassResourceBindings.h>

#include <algorithm>
#include <map>

namespace sky::aurora {
    namespace {
        FrameError Invalid(const char *message)
        {
            return {FrameErrorCode::INVALID_ARGUMENT, message};
        }
        ShaderResourceType BaseType(ShaderResourceType type)
        {
            return type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC ? ShaderResourceType::UNIFORM_BUFFER : type;
        }
        const ShaderBlockLayout *Block(const ShaderReflection &reflection, uint32_t set, uint32_t binding)
        {
            const auto it = std::find_if(reflection.blocks.begin(), reflection.blocks.end(),
                                         [&](const auto &block) { return block.set == set && block.binding == binding; });
            return it == reflection.blocks.end() ? nullptr : &*it;
        }
    } // namespace

    PassResourceBindings::PassResourceBindings(const ShaderReflection &schema) : schema(schema)
    {
    }

    std::optional<FrameError> PassResourceBindings::Check(uint32_t set, uint32_t binding, ShaderResourceType type) const
    {
        const auto matches  = [&](const auto &value) { return value.set == set && value.binding == binding; };
        const auto declared = std::find_if(schema.resources.begin(), schema.resources.end(), matches);
        if (declared == schema.resources.end() || BaseType(declared->type) != BaseType(type))
            return Invalid("Binding slot does not match its shared declaration");
        if (declared->count != 1 || declared->descriptorArray)
            return FrameError{FrameErrorCode::UNSUPPORTED, "Raster binding arrays are not implemented"};
        if (std::any_of(uniforms.begin(), uniforms.end(), [&](const auto &value) { return matches(value.binding); }) ||
            std::any_of(images.begin(), images.end(), matches) || std::any_of(samplers.begin(), samplers.end(), matches))
            return Invalid("Binding slot already has a value");
        return {};
    }

    FrameOperationResult PassResourceBindings::Uniform(ShaderResourceSlot<ShaderResourceType::UNIFORM_BUFFER> slot,
                                                       RasterBufferRange                                      range,
                                                       ShaderResourceType                                     type,
                                                       uint32_t                                               dynamicOffset)
    {
        if (type != ShaderResourceType::UNIFORM_BUFFER && type != ShaderResourceType::UNIFORM_BUFFER_DYNAMIC)
            return Invalid("Uniform requires an explicit uniform binding type");
        if (const auto error = Check(slot.set, slot.binding, type))
            return *error;
        const auto *block = Block(schema, slot.set, slot.binding);
        if (!block || !block->size || range.size < block->size || !range.buffer.graph || range.buffer.index == UINT32_MAX ||
            range.offset > UINT64_MAX - range.size || dynamicOffset > UINT64_MAX - range.offset - range.size ||
            (type == ShaderResourceType::UNIFORM_BUFFER && dynamicOffset))
            return Invalid("Uniform range does not cover its shared block");
        uniforms.push_back({{slot.set, slot.binding, range, dynamicOffset}, type});
        return FrameOperationComplete{};
    }

    FrameOperationResult
    PassResourceBindings::Image(ShaderResourceSlot<ShaderResourceType::SAMPLED_IMAGE> slot, GraphImageHandle image, RasterImageView view)
    {
        if (const auto error = Check(slot.set, slot.binding, ShaderResourceType::SAMPLED_IMAGE))
            return *error;
        if (!image.graph || image.index == UINT32_MAX)
            return Invalid("Sampled binding requires a graph image");
        images.push_back({slot.set, slot.binding, image, view});
        return FrameOperationComplete{};
    }

    FrameOperationResult PassResourceBindings::Sampler(ShaderResourceSlot<ShaderResourceType::SAMPLER> slot, SamplerPtr sampler)
    {
        if (const auto error = Check(slot.set, slot.binding, ShaderResourceType::SAMPLER))
            return *error;
        if (!sampler)
            return Invalid("Sampler binding is missing");
        samplers.push_back({slot.set, slot.binding, std::move(sampler)});
        return FrameOperationComplete{};
    }

    FrameOperationResult PassResourceBindings::Apply(RasterDraw &draw, std::span<const PassResourceBindings *const> groups)
    {
        if (!draw.shader)
            return Invalid("Pass bindings require the draw shader");
        using Key = std::pair<uint32_t, uint32_t>;
        std::map<Key, std::pair<const PassResourceBindings *, const ShaderResource *>> declarations;
        for (const auto *group : groups) {
            if (!group)
                return Invalid("Missing pass bindings group");
            for (const auto &resource : group->schema.resources)
                if (!declarations.emplace(Key{resource.set, resource.binding}, std::pair{group, &resource}).second)
                    return Invalid("Pass declarations overlap at the same binding");
        }
        RasterDraw  staged;
        const auto &reflection = draw.shader->GetReflection();
        for (const auto &resource : reflection.resources) {
            const auto found = declarations.find({resource.set, resource.binding});
            if (found == declarations.end())
                return Invalid("Shader binding is absent from the pass declarations");
            const auto &[group, expected] = found->second;
            if (BaseType(resource.type) != BaseType(expected->type) || resource.count != expected->count ||
                resource.descriptorArray != expected->descriptorArray || resource.texture != expected->texture ||
                resource.sampler != expected->sampler)
                return Invalid("Shader resource shape differs from the pass declarations");
            const auto matches = [&](const auto &value) { return value.set == resource.set && value.binding == resource.binding; };
            switch (resource.type) {
            case ShaderResourceType::UNIFORM_BUFFER:
            case ShaderResourceType::UNIFORM_BUFFER_DYNAMIC: {
                const auto  value = std::find_if(group->uniforms.begin(), group->uniforms.end(), [&](const auto &v) { return matches(v.binding); });
                const auto *actualBlock   = Block(reflection, resource.set, resource.binding);
                const auto *expectedBlock = Block(group->schema, resource.set, resource.binding);
                if (!actualBlock || !expectedBlock || actualBlock->size != expectedBlock->size || actualBlock->members != expectedBlock->members)
                    return Invalid("Shader uniform layout differs from the pass declarations");
                if (value == group->uniforms.end() || value->type != resource.type)
                    return Invalid("Missing uniform binding or incompatible dynamic policy");
                staged.uniforms.push_back(value->binding);
                break;
            }
            case ShaderResourceType::SAMPLED_IMAGE: {
                const auto value = std::find_if(group->images.begin(), group->images.end(), matches);
                if (value == group->images.end())
                    return Invalid("Missing sampled image binding");
                staged.images.push_back(*value);
                break;
            }
            case ShaderResourceType::SAMPLER: {
                const auto value = std::find_if(group->samplers.begin(), group->samplers.end(), matches);
                if (value == group->samplers.end())
                    return Invalid("Missing sampler binding");
                staged.samplers.push_back(*value);
                break;
            }
            default: return FrameError{FrameErrorCode::UNSUPPORTED, "Resource type is not supported by raster bindings"};
            }
        }
        if (const auto error = ValidateRasterBindings(staged, reflection))
            return *error;
        draw.uniforms = std::move(staged.uniforms);
        draw.images   = std::move(staged.images);
        draw.samplers = std::move(staged.samplers);
        return FrameOperationComplete{};
    }
} // namespace sky::aurora

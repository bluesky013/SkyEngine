#include <aurora/renderer/IRenderFeature.h>
#include <aurora/renderer/PassResourceBindings.h>

#include <algorithm>
#include <map>
#include <set>

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
    FrameOperationResult PassResourceBindings::Fail(FrameError error) const
    {
        if (!failure)
            failure = error;
        return error;
    }
    FrameOperationResult PassResourceBindings::UniformData(ShaderResourceSlot<ShaderResourceType::UNIFORM_BUFFER> slot,
                                                           const ShaderCpuSchema                                 &cpu,
                                                           std::span<const std::byte>                             bytes,
                                                           ShaderResourceType                                     type)
    {
        if (const auto error = Check(slot.set, slot.binding, type))
            return Fail(*error);
        if ((type != ShaderResourceType::UNIFORM_BUFFER && type != ShaderResourceType::UNIFORM_BUFFER_DYNAMIC) || bytes.size() != cpu.size ||
            !cpu.size)
            return Fail(Invalid("CPU uniform data requires its exact typed schema"));
        cpuUniforms.push_back({slot.set, slot.binding, type, cpu, {bytes.begin(), bytes.end()}});
        return FrameOperationComplete{};
    }
    FrameOperationResult PassResourceBindings::Data(Name name, const ShaderDataValue &value)
    {
        if (failure)
            return *failure;
        std::string error;
        if (name.Empty() || !ValidateShaderData(value, error))
            return Fail(Invalid("Invalid named shader data"));
        data.emplace_back(name, value);
        return FrameOperationComplete{};
    }
    FrameOperationResult PassResourceBindings::Image(Name name, GraphImageHandle image, RasterImageView view)
    {
        if (failure)
            return *failure;
        if (name.Empty() || !image.graph || image.index == UINT32_MAX)
            return Fail(Invalid("Named image requires an exact graph image"));
        namedImages.push_back({name, image, view});
        return FrameOperationComplete{};
    }
    FrameOperationResult PassResourceBindings::Sampler(Name name, SamplerPtr sampler)
    {
        if (failure)
            return *failure;
        if (name.Empty() || !sampler)
            return Fail(Invalid("Named sampler is missing"));
        namedSamplers.push_back({name, std::move(sampler)});
        return FrameOperationComplete{};
    }

    std::optional<FrameError> PassResourceBindings::Check(uint32_t set, uint32_t binding, ShaderResourceType type) const
    {
        if (failure)
            return failure;
        const auto fail = [&](FrameError error) {
            Fail(error);
            return std::optional<FrameError>(std::move(error));
        };
        const auto matches  = [&](const auto &value) { return value.set == set && value.binding == binding; };
        const auto declared = std::find_if(schema.resources.begin(), schema.resources.end(), matches);
        if (declared == schema.resources.end() || BaseType(declared->type) != BaseType(type))
            return fail(Invalid("Binding slot does not match its shared declaration"));
        if (declared->count != 1 || declared->descriptorArray)
            return fail(FrameError{FrameErrorCode::UNSUPPORTED, "Raster binding arrays are not implemented"});
        if (std::any_of(uniforms.begin(), uniforms.end(), [&](const auto &value) { return matches(value.binding); }) ||
            std::any_of(cpuUniforms.begin(), cpuUniforms.end(), matches) || std::any_of(storage.begin(), storage.end(), matches) ||
            std::any_of(images.begin(), images.end(), matches) || std::any_of(samplers.begin(), samplers.end(), matches))
            return fail(Invalid("Binding slot already has a value"));
        return {};
    }

    FrameOperationResult PassResourceBindings::Uniform(ShaderResourceSlot<ShaderResourceType::UNIFORM_BUFFER> slot,
                                                       RasterBufferRange                                      range,
                                                       ShaderResourceType                                     type,
                                                       uint32_t                                               dynamicOffset)
    {
        if (type != ShaderResourceType::UNIFORM_BUFFER && type != ShaderResourceType::UNIFORM_BUFFER_DYNAMIC)
            return Fail(Invalid("Uniform requires an explicit uniform binding type"));
        if (const auto error = Check(slot.set, slot.binding, type))
            return Fail(*error);
        const auto *block = Block(schema, slot.set, slot.binding);
        if (!block || !block->size || range.size < block->size || !range.buffer.graph || range.buffer.index == UINT32_MAX ||
            range.offset > UINT64_MAX - range.size || dynamicOffset > UINT64_MAX - range.offset - range.size ||
            (type == ShaderResourceType::UNIFORM_BUFFER && dynamicOffset))
            return Fail(Invalid("Uniform range does not cover its shared block"));
        uniforms.push_back({{slot.set, slot.binding, range, dynamicOffset}, type});
        return FrameOperationComplete{};
    }

    FrameOperationResult
    PassResourceBindings::Image(ShaderResourceSlot<ShaderResourceType::SAMPLED_IMAGE> slot, GraphImageHandle image, RasterImageView view)
    {
        if (const auto error = Check(slot.set, slot.binding, ShaderResourceType::SAMPLED_IMAGE))
            return Fail(*error);
        if (!image.graph || image.index == UINT32_MAX)
            return Fail(Invalid("Sampled binding requires a graph image"));
        images.push_back({slot.set, slot.binding, image, view});
        return FrameOperationComplete{};
    }

    FrameOperationResult PassResourceBindings::Sampler(ShaderResourceSlot<ShaderResourceType::SAMPLER> slot, SamplerPtr sampler)
    {
        if (const auto error = Check(slot.set, slot.binding, ShaderResourceType::SAMPLER))
            return Fail(*error);
        if (!sampler)
            return Fail(Invalid("Sampler binding is missing"));
        samplers.push_back({slot.set, slot.binding, std::move(sampler)});
        return FrameOperationComplete{};
    }

    FrameOperationResult PassResourceBindings::Storage(ShaderResourceSlot<ShaderResourceType::STORAGE_BUFFER> slot, RasterBufferRange range)
    {
        if (const auto error = Check(slot.set, slot.binding, ShaderResourceType::STORAGE_BUFFER)) {
            return *error;
        }
        const auto resource =
            std::ranges::find_if(schema.resources, [&](const auto &value) { return value.set == slot.set && value.binding == slot.binding; });
        if (resource->access != ShaderResourceAccess::READ_ONLY || resource->bufferKind != ShaderBufferKind::STRUCTURED || !resource->elementStride ||
            !range.size || range.size % resource->elementStride || range.offset % resource->elementStride || !range.buffer.graph ||
            range.buffer.index == UINT32_MAX || range.offset > UINT64_MAX - range.size) {
            return Fail(Invalid("Read-only storage range does not match its element stride"));
        }
        storage.push_back({slot.set, slot.binding, range, 0});
        return FrameOperationComplete{};
    }

    FrameOperationResult PassResourceBindings::Apply(RasterDraw &draw, std::span<const PassResourceBindings *const> groups)
    {
        return ApplyImpl(nullptr, draw, groups);
    }
    FrameOperationResult
    PassResourceBindings::Apply(FrameGraphBuildContext &context, RasterDraw &draw, std::span<const PassResourceBindings *const> groups)
    {
        return ApplyImpl(&context, draw, groups);
    }
    FrameOperationResult
    PassResourceBindings::ApplyImpl(FrameGraphBuildContext *context, RasterDraw &draw, std::span<const PassResourceBindings *const> groups)
    {
        if (context)
            return ApplyTransaction(*context, draw, groups);
        if (!draw.shader)
            return Invalid("Pass bindings require the draw shader");
        using Key = std::pair<uint32_t, uint32_t>;
        std::map<Key, std::pair<const PassResourceBindings *, const ShaderResource *>> declarations;
        for (const auto *group : groups) {
            if (!group || group->failure)
                return group ? *group->failure : Invalid("Missing pass bindings group");
            if (!group->cpuUniforms.empty() || !group->data.empty() || !group->namedImages.empty() || !group->namedSamplers.empty())
                return Invalid("Typed parameter transactions require a frame allocation context");
            for (const auto &resource : group->schema.resources)
                if (!declarations.emplace(Key{resource.set, resource.binding}, std::pair{group, &resource}).second)
                    return Invalid("Pass declarations overlap at the same binding");
        }
        RasterBindingDraft staged;
        const auto        &reflection = draw.shader->GetReflection();
        for (const auto &resource : reflection.resources) {
            const auto found = declarations.find({resource.set, resource.binding});
            if (found == declarations.end())
                return Invalid("Shader binding is absent from the pass declarations");
            const auto &[group, expected] = found->second;
            if (BaseType(resource.type) != BaseType(expected->type) || resource.count != expected->count ||
                resource.descriptorArray != expected->descriptorArray ||
                ((resource.type == ShaderResourceType::SAMPLED_IMAGE || resource.type == ShaderResourceType::STORAGE_IMAGE) &&
                 resource.texture != expected->texture) ||
                resource.sampler != expected->sampler || resource.bufferKind != expected->bufferKind ||
                resource.elementStride != expected->elementStride || resource.access != expected->access)
                return Invalid("Shader resource shape differs from the pass declarations");
            const auto matches = [&](const auto &value) { return value.set == resource.set && value.binding == resource.binding; };
            if (resource.type == ShaderResourceType::STORAGE_BUFFER) {
                const auto value = std::find_if(group->storage.begin(), group->storage.end(), matches);
                if (value == group->storage.end())
                    return Invalid("Missing read-only storage binding");
                staged.storage.push_back(*value);
            } else if (resource.type == ShaderResourceType::UNIFORM_BUFFER || resource.type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC) {
                const auto  value    = std::ranges::find_if(group->uniforms, [&](const auto &item) { return matches(item.binding); });
                const auto *actual   = Block(reflection, resource.set, resource.binding);
                const auto *declared = Block(group->schema, resource.set, resource.binding);
                if (!actual || !declared || actual->size != declared->size || actual->members != declared->members ||
                    value == group->uniforms.end() || value->type != resource.type)
                    return Invalid("Explicit uniform range requires its exact encoded target layout");
                staged.uniforms.push_back(value->binding);
            } else if (resource.type == ShaderResourceType::SAMPLED_IMAGE) {
                const auto value = std::ranges::find_if(group->images, matches);
                if (value == group->images.end())
                    return Invalid("Missing sampled image binding");
                staged.images.push_back(*value);
            } else if (resource.type == ShaderResourceType::SAMPLER) {
                const auto value = std::ranges::find_if(group->samplers, matches);
                if (value == group->samplers.end())
                    return Invalid("Missing sampler binding");
                staged.samplers.push_back(*value);
            } else {
                return FrameError{FrameErrorCode::UNSUPPORTED, "Resource type is not supported by raster bindings"};
            }
        }
        RasterDraw candidate;
        candidate.shader = draw.shader;
        auto bound       = SetRasterBindings(candidate, std::move(staged));
        if (const auto *error = std::get_if<FrameError>(&bound))
            return *error;
        if (const auto error = ValidateRasterBindings(candidate, reflection))
            return *error;
        draw.groups = std::move(candidate.groups);
        return FrameOperationComplete{};
    }

    FrameOperationResult
    PassResourceBindings::ApplyTransaction(FrameGraphBuildContext &context, RasterDraw &draw, std::span<const PassResourceBindings *const> groups)
    {
        if (!draw.shader || !draw.shader->GetParameterTable().generation)
            return Invalid("Parameter transactions require the linked shader interface");
        const auto &shader     = *draw.shader;
        const auto &reflection = shader.GetReflection();
        using Key              = std::pair<uint32_t, uint32_t>;
        std::map<Key, std::pair<const PassResourceBindings *, const ShaderResource *>> declarations;
        for (const auto *group : groups) {
            if (!group || group->failure)
                return group ? *group->failure : Invalid("Missing pass bindings group");
            for (const auto &resource : group->schema.resources)
                if (!declarations.emplace(Key{resource.set, resource.binding}, std::pair{group, &resource}).second)
                    return Invalid("Pass declarations overlap at the same binding");
        }
        for (const auto &resource : reflection.resources) {
            const auto found = declarations.find({resource.set, resource.binding});
            if (found == declarations.end())
                return Invalid("Shader binding is absent from the pass declarations");
            const auto &expected = *found->second.second;
            if (BaseType(resource.type) != BaseType(expected.type) || resource.count != expected.count ||
                resource.descriptorArray != expected.descriptorArray ||
                ((resource.type == ShaderResourceType::SAMPLED_IMAGE || resource.type == ShaderResourceType::STORAGE_IMAGE) &&
                 resource.texture != expected.texture) ||
                resource.sampler != expected.sampler || resource.bufferKind != expected.bufferKind ||
                resource.elementStride != expected.elementStride || resource.access != expected.access)
                return Invalid("Shader resource shape differs from the pass declarations");
        }
        std::map<uint32_t, std::unique_ptr<ShaderParameterTransaction>> transactions;
        const auto                                                      transaction = [&](uint32_t set) -> ShaderParameterTransaction                                                      &{
            auto &value = transactions[set];
            if (!value)
                value = std::make_unique<ShaderParameterTransaction>(shader, set);
            return *value;
        };
        for (const auto &resource : reflection.resources)
            transaction(resource.set);
        using GraphValue = std::variant<RasterUniformBinding, RasterImageBinding, RasterSamplerBinding>;
        std::vector<GraphValue> registry;
        const auto              add = [&](GraphValue value) {
            const auto token = static_cast<uint32_t>(registry.size());
            registry.push_back(std::move(value));
            return token;
        };
        const auto resolve = [&](const PassResourceBindings &group, Name name) -> const ShaderParameterEntry * {
            std::set<uint32_t> sets;
            for (const auto &resource : group.schema.resources)
                sets.insert(resource.set);
            const ShaderParameterEntry *result = nullptr;
            for (const auto set : sets) {
                ShaderParameterHandle handle;
                const auto            status = shader.GetParameterTable().Resolve(name, set, handle);
                if (status == ShaderParameterLookup::UNKNOWN)
                    continue;
                if ((status != ShaderParameterLookup::READY && status != ShaderParameterLookup::INACTIVE) || result)
                    return nullptr;
                shader.GetParameterTable().Resolve(handle, result);
                if (!result || !declarations.contains({result->set, result->binding}) ||
                    declarations.at({result->set, result->binding}).first != &group)
                    return nullptr;
            }
            return result;
        };
        const auto resourceFor = [&](const PassResourceBindings &group, uint32_t set, uint32_t binding) -> const ShaderResource * {
            const auto declared = declarations.find({set, binding});
            if (declared == declarations.end() || declared->second.first != &group)
                return nullptr;
            const auto matches = [&](const auto &item) { return item.set == set && item.binding == binding; };
            const auto active  = std::ranges::find_if(reflection.resources, matches);
            if (active != reflection.resources.end())
                return &*active;
            const auto inactive = std::ranges::find_if(reflection.declarations, matches);
            return inactive == reflection.declarations.end() ? nullptr : &*inactive;
        };
        for (const auto *group : groups) {
            for (const auto &[name, value] : group->data) {
                const auto *entry = resolve(*group, name);
                if (!entry)
                    return Invalid("Named parameter is unknown, ambiguous or owned by another provider");
                auto &update = transaction(entry->set);
                if (!update.WriteData(name, value))
                    return Invalid(update.GetError().c_str());
            }
            for (const auto &value : group->cpuUniforms) {
                const auto *resource = resourceFor(*group, value.set, value.binding);
                if (!resource)
                    continue;
                if (resource->type != value.type)
                    return Invalid("CPU block has an incompatible binding policy");
                auto &update = transaction(value.set);
                if (!update.WriteBlock(Name(resource->name.c_str()), value.schema, value.bytes))
                    return Invalid(update.GetError().c_str());
            }
            for (const auto &value : group->uniforms) {
                const auto &binding  = value.binding;
                const auto *resource = resourceFor(*group, binding.set, binding.binding);
                if (!resource)
                    continue;
                const auto *actual   = Block(reflection, binding.set, binding.binding);
                const auto *expected = Block(group->schema, binding.set, binding.binding);
                if (!resource || resource->type != value.type || !actual || !expected || actual->size != expected->size ||
                    actual->members != expected->members)
                    return Invalid("Explicit uniform range requires its exact encoded target layout");
                auto &update = transaction(binding.set);
                if (!update.WriteResource(Name(resource->name.c_str()), resource->type, add(binding)))
                    return Invalid(update.GetError().c_str());
            }
            for (const auto &value : group->storage) {
                const auto *resource = resourceFor(*group, value.set, value.binding);
                if (!resource)
                    continue;
                auto &update = transaction(value.set);
                if (!update.WriteResource(Name(resource->name.c_str()), ShaderResourceType::STORAGE_BUFFER, add(value)))
                    return Invalid(update.GetError().c_str());
            }
            for (const auto &value : group->images) {
                const auto *resource = resourceFor(*group, value.set, value.binding);
                if (!resource)
                    continue;
                auto &update = transaction(value.set);
                if (!update.WriteResource(Name(resource->name.c_str()), ShaderResourceType::SAMPLED_IMAGE, add(value)))
                    return Invalid(update.GetError().c_str());
            }
            for (const auto &value : group->samplers) {
                const auto *resource = resourceFor(*group, value.set, value.binding);
                if (!resource)
                    continue;
                if (!value.sampler || value.sampler->GetDevice() != shader.GetDevice())
                    return Invalid("Sampler binding has an invalid interface or device owner");
                auto &update = transaction(value.set);
                if (!update.WriteResource(Name(resource->name.c_str()), ShaderResourceType::SAMPLER, add(value)))
                    return Invalid(update.GetError().c_str());
            }
            for (const auto &value : group->namedImages) {
                const auto *entry = resolve(*group, value.name);
                if (!entry)
                    return Invalid("Named image is outside this provider's shader interface");
                auto &update = transaction(entry->set);
                if (!update.WriteResource(value.name, ShaderResourceType::SAMPLED_IMAGE,
                                          add(RasterImageBinding{entry->set, entry->binding, value.image, value.view})))
                    return Invalid(update.GetError().c_str());
            }
            for (const auto &value : group->namedSamplers) {
                const auto *entry = resolve(*group, value.name);
                if (!entry || !value.sampler || value.sampler->GetDevice() != shader.GetDevice())
                    return Invalid("Named sampler has an invalid interface or device owner");
                auto &update = transaction(entry->set);
                if (!update.WriteResource(value.name, ShaderResourceType::SAMPLER,
                                          add(RasterSamplerBinding{entry->set, entry->binding, value.sampler})))
                    return Invalid(update.GetError().c_str());
            }
        }
        for (const auto &[set, update] : transactions)
            if (!update->Complete())
                return Invalid(update->GetError().c_str());
        RasterDraw staged;
        staged.shader = draw.shader;
        for (const auto &[set, update] : transactions) {
            RasterBindingDraft resources;
            for (const auto &[key, resource] : update->GetState().resources) {
                if (resource.token >= registry.size())
                    return Invalid("Invalid parameter resource token");
                const auto &value = registry[resource.token];
                if (const auto *buffer = std::get_if<RasterUniformBinding>(&value)) {
                    if (resource.type == ShaderResourceType::STORAGE_BUFFER)
                        resources.storage.push_back(*buffer);
                    else
                        resources.uniforms.push_back(*buffer);
                } else if (const auto *image = std::get_if<RasterImageBinding>(&value))
                    resources.images.push_back(*image);
                else
                    resources.samplers.push_back(std::get<RasterSamplerBinding>(value));
            }
            auto materialized = context.MaterializeParameters(draw.shader, set, update->GetState(), std::move(resources));
            if (const auto *error = std::get_if<FrameError>(&materialized))
                return *error;
            staged.groups[set] = std::move(std::get<FrameBindingGroupUse>(materialized));
        }
        if (const auto error = ValidateRasterBindings(staged, reflection))
            return *error;
        draw.groups = std::move(staged.groups);
        return FrameOperationComplete{};
    }
} // namespace sky::aurora

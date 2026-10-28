//
// Created on 2026/10/27.
//

#include <aurora/rdg/FrameBindings.h>

#include <algorithm>
#include <functional>
#include <tuple>

namespace sky::aurora {
    namespace {
        FrameError InvalidBindingGroup(const char *message)
        {
            return {FrameErrorCode::INVALID_ARGUMENT, message};
        }
        template <class Binding>
        const Binding *FindBinding(const std::vector<Binding> &bindings, uint32_t binding)
        {
            const auto found = std::ranges::find_if(bindings, [&](const auto &value) { return value.binding == binding; });
            return found == bindings.end() ? nullptr : &*found;
        }
        FrameOperationResult FreezeGroups(ShaderPtr shader, ComputeBindingDraft draft, FrameBindingGroups &out)
        {
            if (!shader)
                return InvalidBindingGroup("Binding groups require a shader layout owner");
            std::array<FrameBindingGroupData, MAX_SHADER_SETS> data;
            FrameBindingGroups                                 groups;
            for (uint32_t set = 0; set < MAX_SHADER_SETS; ++set) {
                data[set].layoutOwner = shader;
                data[set].set         = set;
            }
            for (auto &binding : draft.buffers) {
                if (binding.set >= MAX_SHADER_SETS || binding.binding >= 32)
                    return InvalidBindingGroup("Buffer group set or binding exceeds the supported range");
                data[binding.set].buffers.push_back(
                    {binding.binding, binding.range, binding.kind, binding.resourceAccess, binding.bufferKind, binding.elementStride});
                if (binding.dynamicOffset || binding.access != FrameAccessMode::READ)
                    groups[binding.set].bufferUses.push_back({binding.binding, binding.dynamicOffset, binding.access});
            }
            for (auto &binding : draft.images) {
                if (binding.set >= MAX_SHADER_SETS || binding.binding >= 32)
                    return InvalidBindingGroup("Image group set or binding exceeds the supported range");
                data[binding.set].images.push_back({binding.binding, binding.image, binding.view, binding.kind});
                if (binding.access != FrameAccessMode::READ)
                    groups[binding.set].imageUses.push_back({binding.binding, binding.access});
            }
            for (auto &binding : draft.samplers) {
                if (binding.set >= MAX_SHADER_SETS || binding.binding >= 32)
                    return InvalidBindingGroup("Sampler group set or binding exceeds the supported range");
                data[binding.set].samplers.push_back({binding.binding, std::move(binding.sampler)});
            }
            for (uint32_t set = 0; set < MAX_SHADER_SETS; ++set)
                if (!data[set].buffers.empty() || !data[set].images.empty() || !data[set].samplers.empty())
                    groups[set].group = std::make_shared<const FrameBindingGroup>(std::move(data[set]));
            if (const auto error = ValidateBindingGroups(shader.Get(), groups))
                return *error;
            out = std::move(groups);
            return FrameOperationComplete{};
        }
    } // namespace

    FrameBindingGroupData FrameBindingGroup::Canonicalize(FrameBindingGroupData data)
    {
        const auto less = [](const auto &left, const auto &right) { return left.binding < right.binding; };
        std::sort(data.buffers.begin(), data.buffers.end(), less);
        std::sort(data.images.begin(), data.images.end(), less);
        std::sort(data.samplers.begin(), data.samplers.end(), less);
        return data;
    }

    std::optional<FrameError> ValidateBindingGroups(const Shader *shader, const FrameBindingGroups &groups) noexcept
    {
        for (uint32_t set = 0; set < MAX_SHADER_SETS; ++set) {
            const auto &use = groups[set];
            if (!use.group) {
                if (!use.bufferUses.empty() || !use.imageUses.empty())
                    return InvalidBindingGroup("Binding overrides require a group snapshot");
                continue;
            }
            const auto &data = use.group->GetData();
            if (!shader || data.layoutOwner.Get() != shader || data.set != set)
                return InvalidBindingGroup("Binding group layout owner or set does not match its use");
            bool       occupied[32]{};
            const auto occupy = [&](uint32_t binding) {
                if (binding >= 32 || occupied[binding])
                    return false;
                occupied[binding] = true;
                return true;
            };
            for (const auto &binding : data.buffers)
                if (!occupy(binding.binding) || binding.kind > FrameBufferBindingKind::STORAGE)
                    return InvalidBindingGroup("Invalid or duplicate buffer group binding");
            for (const auto &binding : data.images)
                if (!occupy(binding.binding) || binding.kind > FrameImageBindingKind::STORAGE)
                    return InvalidBindingGroup("Invalid or duplicate image group binding");
            for (const auto &binding : data.samplers)
                if (!occupy(binding.binding))
                    return InvalidBindingGroup("Invalid or duplicate sampler group binding");
            bool bufferOverrides[32]{}, imageOverrides[32]{};
            for (const auto &binding : use.bufferUses) {
                if (binding.binding >= 32 || bufferOverrides[binding.binding] || binding.access > FrameAccessMode::READ_WRITE ||
                    !FindBinding(data.buffers, binding.binding))
                    return InvalidBindingGroup("Invalid or duplicate buffer group use");
                bufferOverrides[binding.binding] = true;
            }
            for (const auto &binding : use.imageUses) {
                if (binding.binding >= 32 || imageOverrides[binding.binding] || binding.access > FrameAccessMode::READ_WRITE ||
                    !FindBinding(data.images, binding.binding))
                    return InvalidBindingGroup("Invalid or duplicate image group use");
                imageOverrides[binding.binding] = true;
            }
        }
        return {};
    }

    FrameOperationResult SetRasterBindings(RasterDraw &draw, RasterBindingDraft draft)
    {
        ComputeBindingDraft bindings;
        const auto          append = [&](const auto &values, FrameBufferBindingKind kind) {
            for (const auto &value : values) {
                FrameBufferBinding binding;
                binding.set           = value.set;
                binding.binding       = value.binding;
                binding.range         = value.range;
                binding.dynamicOffset = value.dynamicOffset;
                binding.kind          = kind;
                if (draw.shader) {
                    const auto &resources = draw.shader->GetReflection().resources;
                    const auto  resource =
                        std::ranges::find_if(resources, [&](const auto &item) { return item.set == value.set && item.binding == value.binding; });
                    if (resource != resources.end()) {
                        binding.resourceAccess = resource->access;
                        binding.bufferKind     = resource->bufferKind;
                        binding.elementStride  = resource->elementStride;
                    }
                }
                bindings.buffers.push_back(binding);
            }
        };
        append(draft.uniforms, FrameBufferBindingKind::UNIFORM);
        append(draft.storage, FrameBufferBindingKind::STORAGE);
        for (const auto &value : draft.images)
            bindings.images.push_back({value.set, value.binding, value.image, value.view, FrameAccessMode::READ, FrameImageBindingKind::SAMPLED});
        bindings.samplers = std::move(draft.samplers);
        return FreezeGroups(draw.shader, std::move(bindings), draw.groups);
    }

    FrameOperationResult SetComputeBindings(FrameComputePass &pass, ComputeBindingDraft draft)
    {
        return FreezeGroups(pass.shader, std::move(draft), pass.groups);
    }

    static ComputeBindingDraft ResolveGroups(const FrameBindingGroups &groups)
    {
        ComputeBindingDraft result;
        for (uint32_t set = 0; set < MAX_SHADER_SETS; ++set) {
            const auto &use = groups[set];
            if (!use.group)
                continue;
            const auto &data = use.group->GetData();
            for (const auto &binding : data.buffers) {
                const auto *useOverride = FindBinding(use.bufferUses, binding.binding);
                result.buffers.push_back({set, binding.binding, binding.range, useOverride ? useOverride->dynamicOffset : 0,
                                          useOverride ? useOverride->access : FrameAccessMode::READ, binding.kind, binding.resourceAccess,
                                          binding.bufferKind, binding.elementStride});
            }
            for (const auto &binding : data.images) {
                const auto *useOverride = FindBinding(use.imageUses, binding.binding);
                result.images.push_back(
                    {set, binding.binding, binding.image, binding.view, useOverride ? useOverride->access : FrameAccessMode::READ, binding.kind});
            }
            for (const auto &binding : data.samplers)
                result.samplers.push_back({set, binding.binding, binding.sampler});
        }
        return result;
    }

    ComputeBindingDraft ResolveComputeBindings(const FrameComputePass &pass)
    {
        return ResolveGroups(pass.groups);
    }

    RasterBindingDraft ResolveRasterBindings(const RasterDraw &draw)
    {
        const auto         bindings = ResolveGroups(draw.groups);
        RasterBindingDraft result;
        for (const auto &binding : bindings.buffers) {
            auto &values = binding.kind == FrameBufferBindingKind::UNIFORM ? result.uniforms : result.storage;
            values.push_back({binding.set, binding.binding, binding.range, binding.dynamicOffset});
        }
        for (const auto &binding : bindings.images)
            result.images.push_back({binding.set, binding.binding, binding.image, binding.view});
        result.samplers = bindings.samplers;
        return result;
    }

    bool FrameBindingGroupLess::operator()(const FrameBindingGroup *a, const FrameBindingGroup *b) const noexcept
    {
        if (a == b)
            return false;
        if (!a || !b)
            return std::less<const FrameBindingGroup *>{}(a, b);
        const auto &x = a->GetData();
        const auto &y = b->GetData();
        if (x.layoutOwner.Get() != y.layoutOwner.Get())
            return std::less<const Shader *>{}(x.layoutOwner.Get(), y.layoutOwner.Get());
        if (x.set != y.set)
            return x.set < y.set;
        const auto compare = [](const auto &left, const auto &right, const auto &key) {
            return std::lexicographical_compare(left.begin(), left.end(), right.begin(), right.end(),
                                                [&](const auto &l, const auto &r) { return key(l) < key(r); });
        };
        const auto bufferKey = [](const FrameGroupBufferBinding &v) {
            return std::tie(v.binding, v.range.buffer.graph, v.range.buffer.index, v.range.offset, v.range.size, v.kind, v.resourceAccess,
                            v.bufferKind, v.elementStride);
        };
        if (compare(x.buffers, y.buffers, bufferKey))
            return true;
        if (compare(y.buffers, x.buffers, bufferKey))
            return false;
        const auto imageKey = [](const FrameGroupImageBinding &v) {
            return std::tie(v.binding, v.image.graph, v.image.index, v.view.type, v.view.baseMip, v.view.mipCount, v.view.baseLayer,
                            v.view.layerCount, v.kind);
        };
        if (compare(x.images, y.images, imageKey))
            return true;
        if (compare(y.images, x.images, imageKey))
            return false;
        return std::lexicographical_compare(x.samplers.begin(), x.samplers.end(), y.samplers.begin(), y.samplers.end(),
                                            [](const auto &l, const auto &r) {
                                                if (l.binding != r.binding)
                                                    return l.binding < r.binding;
                                                return std::less<const Sampler *>{}(l.sampler.Get(), r.sampler.Get());
                                            });
    }
} // namespace sky::aurora

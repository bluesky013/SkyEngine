//
// Created on 2026/10/17.
//

#include "graph/FrameComposer.h"
#include "frame/ViewportMapping.h"
#include <algorithm>
#include <aurora/rdg/DeviceFrameContext.h>
#include <aurora/rhi/Device.h>
#include <cstring>
#include <set>

namespace sky::aurora {
    namespace {
        bool SupportedResource(ShaderResourceType type, bool compute)
        {
            switch (type) {
            case ShaderResourceType::UNIFORM_BUFFER:
            case ShaderResourceType::UNIFORM_BUFFER_DYNAMIC:
            case ShaderResourceType::SAMPLED_IMAGE:
            case ShaderResourceType::SAMPLER: return true;
            case ShaderResourceType::STORAGE_BUFFER:
            case ShaderResourceType::STORAGE_BUFFER_DYNAMIC:
            case ShaderResourceType::STORAGE_IMAGE: return compute;
            default: return false;
            }
        }
    } // namespace
    std::optional<FrameError> ValidateFrameShader(const RenderTechniqueData &source)
    {
        const auto invalid = [](const char *message) { return FrameError{FrameErrorCode::INVALID_ARGUMENT, message}; };
        const bool compute = source.compute != nullptr;
        if ((compute ? source.vertex || source.fragment : !source.vertex || !source.fragment) || !source.reflection.pushConstants.empty()) {
            return invalid("Technique requires complete graphics or compute artifacts exclusively");
        }
        ShaderReflection linked;
        std::string      message;
        for (const auto &artifact : {source.vertex, source.fragment, source.compute}) {
            if (artifact && (!ValidateShaderArtifact(*artifact, message) || !MergeShaderReflections(artifact->reflection, linked, message))) {
                return FrameError{FrameErrorCode::INVALID_ARGUMENT, std::move(message)};
            }
        }
        for (const auto &policy : source.bindingPolicies) {
            auto found = std::find_if(linked.resources.begin(), linked.resources.end(),
                                      [&](const auto &resource) { return resource.set == policy.set && resource.binding == policy.binding; });
            if (found == linked.resources.end() || found->type != ShaderResourceType::UNIFORM_BUFFER ||
                policy.type != ShaderResourceType::UNIFORM_BUFFER_DYNAMIC)
                return invalid("Invalid technique binding policy");
            found->type = policy.type;
        }
        if (ShaderReflectionFingerprint(linked) != ShaderReflectionFingerprint(source.reflection))
            return invalid("Technique reflection does not match its stage artifacts");
        std::set<std::pair<uint32_t, uint32_t>> resources, blocks;
        for (const auto &resource : source.reflection.resources) {
            if (resource.set >= MAX_SHADER_SETS || resource.count != 1 || resource.descriptorArray ||
                !resources.emplace(resource.set, resource.binding).second)
                return invalid("Technique resource declarations require unique scalar Global/Pass/Batch bindings");
            if (!SupportedResource(resource.type, compute) &&
                !(resource.type == ShaderResourceType::STORAGE_BUFFER && resource.access == ShaderResourceAccess::READ_ONLY &&
                  resource.bufferKind == ShaderBufferKind::STRUCTURED && resource.elementStride)) {
                return invalid("Unsupported technique resource binding type");
            }
        }
        for (const auto &block : source.reflection.blocks) {
            const auto resource = std::find_if(source.reflection.resources.begin(), source.reflection.resources.end(),
                                               [&](const auto &item) { return item.set == block.set && item.binding == block.binding; });
            if (!block.size || resource == source.reflection.resources.end() ||
                (resource->type != ShaderResourceType::UNIFORM_BUFFER && resource->type != ShaderResourceType::UNIFORM_BUFFER_DYNAMIC) ||
                !blocks.emplace(block.set, block.binding).second)
                return invalid("Technique block has no matching uniform resource");
            if (std::ranges::any_of(block.members, [&](const auto &member) {
                    return !member.size || member.offset > block.size || member.size > block.size - member.offset;
                })) {
                return invalid("Technique uniform member exceeds its reflected block");
            }
        }
        for (const auto &resource : source.reflection.resources)
            if ((resource.type == ShaderResourceType::UNIFORM_BUFFER || resource.type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC) &&
                !blocks.contains({resource.set, resource.binding}))
                return invalid("Technique uniform resource requires a reflected block");
        return {};
    }
    std::variant<FrameError, ShaderPtr> CreateFrameShader(Device &device, const RenderTechniqueData &source)
    {
        if (const auto error = ValidateFrameShader(source))
            return *error;
        const bool compute = source.compute != nullptr;
        if (source.backend != device.GetAPI()) {
            return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Shader artifact backend does not match the active device"};
        }
        const auto function = [&](ShaderStageFlagBit stage, const ShaderStageArtifactPtr &artifact) {
            ShaderFunction::Descriptor desc{};
            desc.stage    = stage;
            desc.artifact = artifact;
            return ShaderFunctionPtr(device.CreateShaderFunction(desc));
        };
        const auto vs = compute ? ShaderFunctionPtr{} : function(ShaderStageFlagBit::VS, source.vertex);
        const auto fs = compute ? ShaderFunctionPtr{} : function(ShaderStageFlagBit::FS, source.fragment);
        const auto cs = compute ? function(ShaderStageFlagBit::CS, source.compute) : ShaderFunctionPtr{};
        if (compute ? !cs : !vs || !fs) {
            return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Shader function creation failed"};
        }
        Shader::Descriptor desc{};
        desc.vs = vs.Get();
        desc.ps = fs.Get();
        if (compute) {
            desc.cs = cs.Get();
        }
        desc.reflection       = &source.reflection;
        desc.bindingPolicies  = source.bindingPolicies;
        desc.parameterAliases = source.parameterAliases;
        desc.specialization   = &source.specialization;
        ShaderPtr shader(device.CreateShader(desc));
        if (!shader)
            return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Shader layout creation failed"};
        return shader;
    }
    FrameComposer::FrameComposer(Device &device, DeviceFrameContext &context, FrameGraphBuilder &builder, const FrameBuildInputs &inputs)
        : device(device), context(context), builder(builder), inputs(inputs)
    {
    }
    void FrameComposer::AddImage(RenderResourceKey key, GraphImageHandle image)
    {
        images.emplace_back(key, image);
    }
    std::optional<GraphImageHandle> FrameComposer::ResolveImage(const RenderResourceKey &key) const
    {
        const auto found = std::find_if(images.begin(), images.end(), [&](const auto &entry) { return entry.first == key; });
        return found == images.end() ? std::optional<GraphImageHandle>{} : found->second;
    }
    void FrameComposer::AddOutput(FrameRenderTarget target, bool sampleable)
    {
        target.logicalToClip = ViewportMapping{target.output.extent, target.transform}.ClipTransform();
        if (device.GetAPI() == API::DX12 || device.GetAPI() == API::METAL) {
            target.logicalToClip.v[1] *= -1;
            target.logicalToClip.v[5] *= -1;
        }
        outputs.push_back({target, sampleable});
    }
    const FrameBuildInputs &FrameComposer::GetInputs() const
    {
        return inputs;
    }
    std::optional<FrameRenderTarget> FrameComposer::GetOutput(ViewportId id) const
    {
        const auto found = std::find_if(outputs.begin(), outputs.end(), [&](const auto &output) { return output.target.output.viewport == id; });
        return found == outputs.end() ? std::optional<FrameRenderTarget>{} : found->target;
    }
    std::optional<GraphImageHandle> FrameComposer::ResolveViewOutput(ViewOutputRef ref) const
    {
        const auto found = std::find_if(outputs.begin(), outputs.end(), [&](const auto &output) {
            return output.sampleable && output.target.initialized && output.target.output.viewport == ref.viewport &&
                   output.target.revision == ref.revision;
        });
        return found == outputs.end() ? std::optional<GraphImageHandle>{} : found->target.color;
    }
    std::variant<FrameError, ShaderPtr> FrameComposer::CreateShader(const RenderTechniqueData &source)
    {
        return CreateFrameShader(device, source);
    }
    std::variant<FrameError, SamplerPtr> FrameComposer::CreateSampler(const Sampler::Descriptor &desc)
    {
        SamplerPtr sampler(device.CreateSampler(desc));
        if (!sampler)
            return FrameError{FrameErrorCode::OUT_OF_MEMORY, "Feature sampler creation failed"};
        return sampler;
    }
    std::variant<FrameError, RasterBufferRange> FrameComposer::Upload(std::span<const std::byte> bytes, BufferUsageFlags usage)
    {
        if (usage == BufferUsageFlagBit::STORAGE) {
            if (bytes.empty() || bytes.size() % 4) {
                return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Storage upload requires nonempty four-byte aligned data"};
            }
            auto written = context.WriteFrameBuffer(inputs.lease, FrameBufferKind::STORAGE, bytes, 64);
            if (const auto *error = std::get_if<FrameError>(&written)) {
                return *error;
            }
            const auto &slice    = std::get<FrameBufferSlice>(written);
            auto        imported = builder.Import(slice.buffer, CopyBufferState::HOST_WRITE, CopyBufferState::HOST_WRITE, true);
            if (const auto *error = std::get_if<FrameError>(&imported)) {
                return *error;
            }
            return RasterBufferRange{std::get<GraphBufferHandle>(imported), slice.offset, bytes.size()};
        }
        if (bytes.empty() || bytes.size() % 4 || !(usage & (BufferUsageFlagBit::VERTEX | BufferUsageFlagBit::INDEX)))
            return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Feature geometry requires aligned vertex/index bytes"};
        auto written = context.WriteFrameBuffer(inputs.lease, FrameBufferKind::STAGING, bytes, 4);
        if (const auto *error = std::get_if<FrameError>(&written))
            return *error;
        const auto &slice   = std::get<FrameBufferSlice>(written);
        auto        staging = builder.Import(slice.buffer, CopyBufferState::HOST_WRITE, CopyBufferState::HOST_WRITE, true);
        if (const auto *error = std::get_if<FrameError>(&staging))
            return *error;
        Buffer::Descriptor desc;
        desc.size   = bytes.size();
        desc.usage  = usage | BufferUsageFlagBit::TRANSFER_DST;
        desc.memory = MemoryType::GPU_ONLY;
        auto target = builder.CreateTransient(desc);
        if (const auto *error = std::get_if<FrameError>(&target))
            return *error;
        const auto handle = std::get<GraphBufferHandle>(target);
        auto       copied = builder.Copy(BufferCopyPass{std::get<GraphBufferHandle>(staging), handle, slice.offset, 0, slice.size});
        if (const auto *error = std::get_if<FrameError>(&copied))
            return *error;
        return RasterBufferRange{handle, 0, slice.size};
    }
    std::variant<FrameError, RasterUniformBinding>
    FrameComposer::WriteUniform(uint32_t set, uint32_t binding, std::span<const std::byte> bytes, ShaderResourceType type)
    {
        if (set >= MAX_SHADER_SETS || binding >= 32 ||
            (type != ShaderResourceType::UNIFORM_BUFFER && type != ShaderResourceType::UNIFORM_BUFFER_DYNAMIC))
            return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Uniform allocation requires an explicit uniform binding type"};
        const auto available = context.AvailableFrameBufferBytes(inputs.lease, FrameBufferKind::UNIFORM);
        if (const auto *error = std::get_if<FrameError>(&available))
            return *error;
        if (const auto found = uniformUploads.find(UniformUploadView{set, binding, type, bytes}); found != uniformUploads.end())
            return found->second;
        if (bytes.empty())
            return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Uniform upload is empty"};
        if (bytes.size() > std::get<uint64_t>(available))
            return FrameError{FrameErrorCode::OUT_OF_MEMORY, "Frame uniform allocation budget exhausted"};
        auto written = context.WriteFrameBuffer(inputs.lease, FrameBufferKind::UNIFORM, bytes);
        if (const auto *error = std::get_if<FrameError>(&written))
            return *error;
        const auto &slice   = std::get<FrameBufferSlice>(written);
        const bool  dynamic = type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC;
        if (dynamic && slice.offset > UINT32_MAX)
            return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Uniform offset exceeds the dynamic offset range"};
        auto imported = builder.Import(slice.buffer, CopyBufferState::HOST_WRITE, CopyBufferState::HOST_WRITE, true);
        if (const auto *error = std::get_if<FrameError>(&imported))
            return *error;
        RasterUniformBinding result{set,
                                    binding,
                                    {std::get<GraphBufferHandle>(imported), dynamic ? 0 : slice.offset, bytes.size()},
                                    dynamic ? static_cast<uint32_t>(slice.offset) : 0};
        if (uniformUploads.size() < 1024)
            uniformUploads.emplace(UniformUploadKey{set, binding, type, {bytes.begin(), bytes.end()}}, result);
        return result;
    }
    bool FrameComposer::ParameterGroupLess::operator()(const ParameterGroupKey &a, const ParameterGroupKey &b) const
    {
        if (a.shader.Get() != b.shader.Get())
            return std::less<const Shader *>{}(a.shader.Get(), b.shader.Get());
        if (a.set != b.set)
            return a.set < b.set;
        if (a.fields != b.fields)
            return a.fields < b.fields;
        return FrameBindingGroupLess{}(a.resources.get(), b.resources.get());
    }
    std::variant<FrameError, FrameBindingGroupUse>
    FrameComposer::MaterializeParameters(ShaderPtr shader, uint32_t set, const ShaderParameterTransactionState &state, RasterBindingDraft resources)
    {
        if (!shader || shader->GetDevice() != &device || set >= MAX_SHADER_SETS)
            return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Invalid parameter group shader/set"};
        const auto available = context.AvailableFrameBufferBytes(inputs.lease, FrameBufferKind::UNIFORM);
        if (const auto *error = std::get_if<FrameError>(&available))
            return *error;
        if (auto error = ValidateParameterResources(shader, set, state, resources))
            return *error;
        RasterDraw key;
        key.shader       = shader;
        const auto bound = SetRasterBindings(key, resources);
        if (const auto *error = std::get_if<FrameError>(&bound))
            return *error;
        const auto       &resourceKey = key.groups[set];
        ParameterGroupKey cacheKey{shader, set, state.fields, resourceKey.group};
        if (const auto entry = parameterGroups.find(cacheKey); entry != parameterGroups.end()) {
            auto result = entry->second;
            if (resourceKey.group) {
                const auto &buffers = resourceKey.group->GetData().buffers;
                std::erase_if(result.bufferUses, [&](const auto &use) {
                    return std::ranges::any_of(buffers, [&](const auto &buffer) { return buffer.binding == use.binding; });
                });
            }
            for (const auto &use : resourceKey.bufferUses) {
                const auto found =
                    std::find_if(result.bufferUses.begin(), result.bufferUses.end(), [&](const auto &value) { return value.binding == use.binding; });
                if (found == result.bufferUses.end())
                    result.bufferUses.push_back(use);
                else
                    *found = use;
            }
            result.imageUses = resourceKey.imageUses;
            return result;
        }
        auto materialized = FrameGraphServices::MaterializeParameters(shader, set, state, std::move(resources));
        if (auto *group = std::get_if<FrameBindingGroupUse>(&materialized); group && parameterGroups.size() < 1024)
            parameterGroups.emplace(std::move(cacheKey), *group);
        return materialized;
    }
    void FrameComposer::SetPreparedView(const PreparedViewData                     *prepared,
                                        const FrozenResourceCatalog                &catalog,
                                        std::span<const PreparedMeshLayout>         meshLayouts,
                                        std::span<const PreparedMaterialTechniques> materialLayouts,
                                        std::span<const PreparedGraphBuffer>        inGraph)
    {
        view         = prepared;
        resources    = &catalog;
        meshes       = meshLayouts;
        materials    = materialLayouts;
        localBuffers = inGraph;
    }
    std::variant<FrameError, std::vector<ResolvedRasterDraw>>
    FrameComposer::ResolveDraws(Name tag, Rect2D scissor, PreparedDrawList list, const ShaderVariant *pipelineValues)
    {
        if (!view || !resources)
            return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Prepared draws require an active BuildView snapshot"};
        if (tag.Empty() || (list != PreparedDrawList::VISIBLE && list != PreparedDrawList::SHADOW_CASTERS)) {
            return FrameError{.code = FrameErrorCode::INVALID_ARGUMENT, .message = "Scene queue requires a Tag and a known draw list"};
        }
        PreparedViewData                    selected = *view;
        auto                               &packets  = list == PreparedDrawList::VISIBLE ? selected.draws : selected.shadowCasters;
        std::vector<PreparedMaterialLayout> layouts;
        for (const auto &material : materials)
            for (const auto &[candidate, layout] : material.techniques)
                if (candidate == tag)
                    layouts.push_back(layout);
        std::erase_if(packets, [&](const auto &packet) {
            return std::none_of(layouts.begin(), layouts.end(), [&](const auto &layout) { return layout.resource == packet.material.resource; });
        });
        const ShaderVariant empty;
        auto resolved = ResolvePreparedRaster(device, context, inputs.lease, builder, selected, *resources, meshes, layouts, scissor, localBuffers,
                                              list, pipelineValues ? *pipelineValues : empty);
        if (const auto *error = std::get_if<FrameError>(&resolved))
            return *error;
        return std::move(std::get<PreparedRasterView>(resolved).draws);
    }
    std::variant<FrameError, GraphBufferHandle> FrameComposer::CreateBuffer(const Buffer::Descriptor &desc)
    {
        return builder.CreateTransient(desc);
    }
    std::variant<FrameError, GraphImageHandle> FrameComposer::CreateImage(const Image::Descriptor &desc)
    {
        return builder.CreateTransient(desc);
    }
    FrameOperationResult FrameComposer::Copy(BufferCopyPass pass)
    {
        return builder.Copy(pass);
    }
    FrameOperationResult FrameComposer::Copy(BufferImageCopyPass pass)
    {
        return builder.Copy(pass);
    }
    FrameOperationResult FrameComposer::AddRaster(FrameRasterPass pass, bool fullscreen)
    {
        const auto color  = pass.color;
        auto       result = fullscreen ? builder.Fullscreen(std::move(pass)) : builder.Raster(std::move(pass));
        if (std::holds_alternative<FrameOperationComplete>(result))
            for (auto &output : outputs)
                if (output.target.color == color)
                    output.target.initialized = true;
        return result;
    }
    FrameOperationResult FrameComposer::Raster(FrameRasterPass pass)
    {
        return AddRaster(std::move(pass), false);
    }
    FrameOperationResult FrameComposer::Fullscreen(FrameRasterPass pass)
    {
        return AddRaster(std::move(pass), true);
    }
    FrameOperationResult FrameComposer::Compute(FrameComputePass pass)
    {
        return builder.Compute(std::move(pass));
    }
    FrameOperationResult FrameComposer::Custom(FrameCustomPass pass)
    {
        return builder.Custom(std::move(pass));
    }
    FrameOperationResult FrameComposer::Export(GraphBufferExport value)
    {
        return builder.Export(value);
    }
    FrameOperationResult FrameComposer::Export(GraphImageHandle value)
    {
        return builder.Export(value);
    }
    FrameOperationResult FrameComposer::ExportOutputs()
    {
        for (const auto &output : outputs)
            if (output.target.initialized) {
                const auto result = builder.Export(output.target.color);
                if (const auto *error = std::get_if<FrameError>(&result))
                    return *error;
            }
        return FrameOperationComplete{};
    }
} // namespace sky::aurora

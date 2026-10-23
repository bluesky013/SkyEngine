//
// Created on 2026/10/23.
//

#include <aurora/rhi/ResourceGroup.h>

#include <algorithm>
#include <aurora/rhi/Device.h>
#include <variant>

namespace sky::aurora {
    ShaderParameterWriteResult DescriptorEncoder::WriteData(Name, const ShaderDataValue &)
    {
        return {};
    }
    ShaderParameterWriteResult DescriptorEncoder::WriteData(ShaderParameterHandle, const ShaderDataValue &)
    {
        return {};
    }
    ShaderParameterWriteResult DescriptorEncoder::WriteBlock(Name, const ShaderCpuSchema &, std::span<const std::byte>)
    {
        return {};
    }
    ShaderParameterWriteResult DescriptorEncoder::WriteBuffer(Name, Buffer *, uint64_t, uint64_t, uint32_t)
    {
        return {};
    }
    ShaderParameterWriteResult DescriptorEncoder::WriteImage(Name, Image *, ImageLayout, uint32_t)
    {
        return {};
    }
    ShaderParameterWriteResult DescriptorEncoder::WriteSampler(Name, Sampler *, uint32_t)
    {
        return {};
    }
    const std::string &DescriptorEncoder::GetError() const
    {
        static const std::string unsupported("Named updates require an update context");
        return unsupported;
    }
    CounterPtr<ResourceGroup> DescriptorEncoder::GetResult() const
    {
        return {};
    }
    void DescriptorEncoder::Cancel()
    {
    }

    bool ResourceGroup::InitParameterLayout(const Descriptor &desc)
    {
        if (!desc.shader || desc.shader->GetDevice() != GetDevice() || !desc.shader->GetParameterTable().generation)
            return false;
        parameterShader = desc.shader;
        parameterSet    = desc.set;
        parameterState  = std::make_shared<const ShaderParameterState>();
        return true;
    }

    class NamedDescriptorEncoder final : public DescriptorEncoder {
    public:
        NamedDescriptorEncoder(const ResourceGroup &group, ShaderParameterUpdateContext &context)
            : shader(group.parameterShader), set(group.parameterSet), context(context), owner(group.GetDevice())
        {
            if (group.parameterState)
                staged = *group.parameterState;
            if (!shader || !owner) {
                error = "Invalid parameter shader owner";
                return;
            }
            ShaderParameterTransactionState initial;
            initial.fields     = staged.fields;
            const auto inherit = [&](const auto &values, ShaderResourceType fallback) {
                for (const auto &[key, value] : values) {
                    const auto *resource = Declaration(key.first);
                    const auto  type     = resource ? resource->type : fallback;
                    const auto  token    = static_cast<uint32_t>(registry.size());
                    registry.emplace_back(value);
                    initial.resources[key] = {key.first, key.second, token, type};
                }
            };
            inherit(staged.buffers, ShaderResourceType::STORAGE_BUFFER);
            inherit(staged.images, ShaderResourceType::SAMPLED_IMAGE);
            inherit(staged.samplers, ShaderResourceType::SAMPLER);
            transaction = std::make_unique<ShaderParameterTransaction>(*shader, set, initial);
            if (context.GetDevice() != owner || !context.IsValid())
                Fail("Invalid parameter update context");
        }
        ShaderParameterWriteResult WriteData(Name name, const ShaderDataValue &value) override
        {
            return Open() ? transaction->WriteData(name, value) : Fail("Parameter transaction is closed or failed");
        }
        ShaderParameterWriteResult WriteData(ShaderParameterHandle handle, const ShaderDataValue &value) override
        {
            return Open() ? transaction->WriteData(handle, value) : Fail("Parameter transaction is closed or failed");
        }
        ShaderParameterWriteResult WriteBlock(Name name, const ShaderCpuSchema &schema, std::span<const std::byte> bytes) override
        {
            return Open() ? transaction->WriteBlock(name, schema, bytes) : Fail("Parameter transaction is closed or failed");
        }
        ShaderParameterWriteResult WriteBuffer(Name name, Buffer *buffer, uint64_t offset, uint64_t range, uint32_t element) override
        {
            if (!Open())
                return Fail("Parameter transaction is closed or failed");
            const auto token  = static_cast<uint32_t>(registry.size());
            const auto result = transaction->WriteResource(name, ShaderResourceType::STORAGE_BUFFER, token, element);
            if (!result)
                return result;
            const auto *resource = Declaration(name);
            if (!resource || !buffer || buffer->GetDevice() != owner || !buffer->GetDescriptor().usage.TestBit(BufferUsageFlagBit::STORAGE) ||
                !range || offset > buffer->GetSize() || range > buffer->GetSize() - offset)
                return Fail("Invalid named storage buffer type, owner, usage or range");
            const auto stride = resource->bufferKind == ShaderBufferKind::RAW ? 4 : resource->elementStride;
            if (!stride || offset % stride || range % stride)
                return Fail("Invalid storage buffer stride");
            if (result.status == ShaderParameterWriteStatus::INACTIVE)
                return result;
            ShaderParameterBufferValue value{buffer, offset, range};
            registry.emplace_back(value);
            staged.buffers[{resource->binding, element}] = std::move(value);
            return result;
        }
        ShaderParameterWriteResult WriteImage(Name name, Image *image, ImageLayout layout, uint32_t element) override
        {
            if (!Open())
                return Fail("Parameter transaction is closed or failed");
            const auto *resource = Declaration(name);
            const auto  type     = resource ? resource->type : ShaderResourceType::SAMPLED_IMAGE;
            const auto  token    = static_cast<uint32_t>(registry.size());
            const auto  result   = transaction->WriteResource(name, type, token, element);
            if (!result)
                return result;
            if (!resource || (resource->type != ShaderResourceType::SAMPLED_IMAGE && resource->type != ShaderResourceType::STORAGE_IMAGE) || !image ||
                image->GetDevice() != owner)
                return Fail("Invalid named image type or owner");
            const auto &desc  = image->GetDescriptor();
            const auto &shape = resource->texture;
            const bool  unsignedFormat =
                desc.format == PixelFormat::R8_UINT || (desc.format >= PixelFormat::R32_UINT && desc.format <= PixelFormat::RGBA32_UINT);
            const auto usage = resource->type == ShaderResourceType::STORAGE_IMAGE ? ImageUsageFlagBit::STORAGE : ImageUsageFlagBit::SAMPLED;
            if (shape.dimension != ShaderTextureDimension::D2 || shape.scalarType != ShaderScalarType::FLOAT || shape.multisampled ||
                desc.format == PixelFormat::UNDEFINED || unsignedFormat || desc.imageType != ImageType::IMAGE_2D || desc.samples != SampleCount::X1 ||
                shape.arrayed != (desc.arrayLayers > 1) || !(desc.usage & usage) ||
                (resource->type == ShaderResourceType::STORAGE_IMAGE ? layout != ImageLayout::GENERAL : layout != ImageLayout::SHADER_READ_ONLY))
                return Fail("Unsupported named image shape, view or layout");
            if (result.status == ShaderParameterWriteStatus::INACTIVE)
                return result;
            ShaderParameterImageValue value{image, layout};
            registry.emplace_back(value);
            staged.images[{resource->binding, element}] = std::move(value);
            return result;
        }
        ShaderParameterWriteResult WriteSampler(Name name, Sampler *sampler, uint32_t element) override
        {
            if (!Open())
                return Fail("Parameter transaction is closed or failed");
            const auto token  = static_cast<uint32_t>(registry.size());
            const auto result = transaction->WriteResource(name, ShaderResourceType::SAMPLER, token, element);
            if (!result)
                return result;
            const auto *resource = Declaration(name);
            if (!resource || resource->sampler != ShaderSamplerType::REGULAR || !sampler || sampler->GetDevice() != owner)
                return Fail("Invalid named sampler semantic type or owner");
            if (result.status == ShaderParameterWriteStatus::INACTIVE)
                return result;
            SamplerPtr value(sampler);
            registry.emplace_back(value);
            staged.samplers[{resource->binding, element}] = std::move(value);
            return result;
        }
        bool WriteBuffer(uint32_t, Buffer *, uint64_t, uint64_t, uint32_t) override
        {
            return static_cast<bool>(Fail("Named transaction requires a parameter name"));
        }
        bool WriteImage(uint32_t, Image *, ImageLayout, uint32_t) override
        {
            return static_cast<bool>(Fail("Named transaction requires a parameter name"));
        }
        bool WriteSampler(uint32_t, Sampler *, uint32_t) override
        {
            return static_cast<bool>(Fail("Named transaction requires a parameter name"));
        }
        bool End() override
        {
            if (!Open() || !context.IsValid() || context.GetDevice() != owner) {
                Fail("Parameter transaction or lease is no longer valid");
                return false;
            }
            std::shared_ptr<const ShaderParameterPacket> packet;
            if (!transaction->Finalize(packet))
                return false;
            ResourceGroupPtr candidate(context.GetDevice()->CreateResourceGroup({shader.Get(), set}));
            if (!candidate || candidate->GetDevice() != owner) {
                Fail("Parameter snapshot allocation failed");
                return false;
            }
            auto native = candidate->CreateEncoder();
            if (!native) {
                Fail("Parameter snapshot has no native encoder");
                return false;
            }
            staged.uniforms.clear();
            for (const auto &uniform : packet->uniforms) {
                ShaderUniformAllocation allocation;
                if (!context.UploadUniform(uniform.bytes, uniform.alignment, allocation, error)) {
                    Fail(error.empty() ? "Uniform allocation failed" : error);
                    return false;
                }
                if (!context.IsValid() || !allocation.buffer || !allocation.retention || allocation.buffer->GetDevice() != owner ||
                    allocation.offset % uniform.alignment || allocation.range < uniform.bytes.size() ||
                    allocation.offset > allocation.buffer->GetSize() || allocation.range > allocation.buffer->GetSize() - allocation.offset ||
                    !native->WriteBuffer(uniform.binding, allocation.buffer.Get(), allocation.offset, uniform.bytes.size())) {
                    Fail("Invalid uploaded uniform allocation");
                    return false;
                }
                staged.uniforms.push_back(std::move(allocation));
            }
            for (const auto &resource : packet->resources) {
                if (resource.token >= registry.size()) {
                    Fail("Parameter resource token has no native value");
                    return false;
                }
                const auto &value   = registry[resource.token];
                bool        written = false;
                if (resource.type == ShaderResourceType::STORAGE_BUFFER || resource.type == ShaderResourceType::STORAGE_BUFFER_DYNAMIC) {
                    if (const auto *buffer = std::get_if<ShaderParameterBufferValue>(&value))
                        written = native->WriteBuffer(resource.binding, buffer->buffer.Get(), buffer->offset, buffer->range, resource.element);
                } else if (resource.type == ShaderResourceType::SAMPLED_IMAGE || resource.type == ShaderResourceType::STORAGE_IMAGE) {
                    if (const auto *image = std::get_if<ShaderParameterImageValue>(&value))
                        written = native->WriteImage(resource.binding, image->image.Get(), image->layout, resource.element);
                } else if (resource.type == ShaderResourceType::SAMPLER) {
                    if (const auto *sampler = std::get_if<SamplerPtr>(&value))
                        written = native->WriteSampler(resource.binding, sampler->Get(), resource.element);
                }
                if (!written) {
                    Fail("Native parameter binding failed");
                    return false;
                }
            }
            if (!native->End() || !context.IsValid()) {
                Fail("Native parameter commit failed or lease expired");
                return false;
            }
            staged.fields                = transaction->GetState().fields;
            candidate->parameterState    = std::make_shared<const ShaderParameterState>(std::move(staged));
            candidate->parameterSnapshot = true;
            result                       = std::move(candidate);
            ended                        = true;
            return true;
        }
        const std::string &GetError() const override
        {
            return transaction ? transaction->GetError() : error;
        }
        ResourceGroupPtr GetResult() const override
        {
            return result;
        }
        void Cancel() override
        {
            if (transaction)
                transaction->Cancel();
            ended  = true;
            staged = {};
            registry.clear();
            result = {};
        }

    private:
        bool Open() const
        {
            return !ended && transaction && transaction->GetError().empty();
        }
        ShaderParameterWriteResult Fail(std::string message)
        {
            if (transaction)
                transaction->Fail(std::move(message));
            else if (error.empty())
                error = std::move(message);
            return {};
        }
        const ShaderResource *Declaration(uint32_t binding) const
        {
            const auto &resources = shader->GetReflection().declarations;
            const auto  found     = std::ranges::find_if(resources, [&](const auto &item) { return item.set == set && item.binding == binding; });
            return found == resources.end() ? nullptr : &*found;
        }
        const ShaderResource *Declaration(Name name) const
        {
            ShaderParameterHandle handle;
            const auto            lookup = shader->GetParameterTable().Resolve(name, set, handle);
            if (lookup != ShaderParameterLookup::READY && lookup != ShaderParameterLookup::INACTIVE)
                return nullptr;
            const ShaderParameterEntry *entry = nullptr;
            shader->GetParameterTable().Resolve(handle, entry);
            return entry && entry->kind == ShaderParameterKind::RESOURCE ? Declaration(entry->binding) : nullptr;
        }
        using NativeValue = std::variant<ShaderParameterBufferValue, ShaderParameterImageValue, SamplerPtr>;
        ShaderPtr                                   shader;
        uint32_t                                    set = 0;
        ShaderParameterUpdateContext               &context;
        const Device                               *owner;
        ShaderParameterState                        staged;
        std::unique_ptr<ShaderParameterTransaction> transaction;
        std::vector<NativeValue>                    registry;
        ResourceGroupPtr                            result;
        std::string                                 error;
        bool                                        ended = false;
    };

    std::unique_ptr<DescriptorEncoder> ResourceGroup::CreateEncoder(ShaderParameterUpdateContext &context) const
    {
        return std::make_unique<NamedDescriptorEncoder>(*this, context);
    }
} // namespace sky::aurora

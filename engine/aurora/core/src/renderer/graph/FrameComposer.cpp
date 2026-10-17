//
// Created on 2026/10/17.
//

#include "graph/FrameComposer.h"
#include "frame/ViewportMapping.h"
#include <algorithm>
#include <aurora/rdg/DeviceFrameContext.h>
#include <aurora/rhi/Device.h>
#include <cstring>

namespace sky::aurora {
    std::variant<FrameError, ShaderPtr> CreateFrameShader(Device &device, const RenderMaterialData &source)
    {
        if (source.backend != device.GetAPI() || source.vertexCode.empty() || source.fragmentCode.empty() || source.vertexCode.size() > UINT32_MAX ||
            source.fragmentCode.size() > UINT32_MAX || source.vertexEntry.empty() || source.fragmentEntry.empty())
            return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Shader code does not match the active backend"};
        const auto function = [&](ShaderStageFlagBit stage, const std::vector<uint8_t> &bytes, const std::string &entry) {
            CounterPtr<ShaderBinaryProvider> provider(new ShaderBinaryProvider());
            provider->binaryData = BinaryDataPtr(new BinaryData(static_cast<uint32_t>(bytes.size())));
            std::memcpy(provider->binaryData->Data(), bytes.data(), bytes.size());
            return ShaderFunctionPtr(device.CreateShaderFunction({stage, provider, entry}));
        };
        const auto vs = function(ShaderStageFlagBit::VS, source.vertexCode, source.vertexEntry);
        const auto fs = function(ShaderStageFlagBit::FS, source.fragmentCode, source.fragmentEntry);
        if (!vs || !fs)
            return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Shader function creation failed"};
        Shader::Descriptor desc{};
        desc.vs         = vs.Get();
        desc.ps         = fs.Get();
        desc.reflection = &source.reflection;
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
        if (device.GetAPI() == API::DX12) {
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
    std::variant<FrameError, ShaderPtr> FrameComposer::CreateShader(const RenderMaterialData &source)
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
    std::variant<FrameError, RasterUniformBinding> FrameComposer::WriteUniform(uint32_t set, uint32_t binding, std::span<const std::byte> bytes)
    {
        auto written = context.WriteFrameBuffer(inputs.lease, FrameBufferKind::UNIFORM, bytes);
        if (const auto *error = std::get_if<FrameError>(&written))
            return *error;
        const auto &slice    = std::get<FrameBufferSlice>(written);
        auto        imported = builder.Import(slice.buffer, CopyBufferState::HOST_WRITE, CopyBufferState::HOST_WRITE, true);
        if (const auto *error = std::get_if<FrameError>(&imported))
            return *error;
        return RasterUniformBinding{set, binding, {std::get<GraphBufferHandle>(imported), slice.offset, slice.size}};
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

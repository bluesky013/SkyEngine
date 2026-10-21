#include "graph/FrameGraphBuilder.h"

#include <core/environment/Singleton.h>
#include <mutex>

namespace sky::aurora {
    class FrameGraphIds : public Singleton<FrameGraphIds> {
    public:
        uint64_t Next()
        {
            std::lock_guard lock(mutex);
            return last == UINT64_MAX ? 0 : ++last;
        }

    private:
        std::mutex mutex;
        uint64_t   last = 0;
    };

    std::variant<FrameError, GraphBufferHandle> FrameGraphBuilder::CreateTransient(Buffer::Descriptor descriptor) noexcept
    {
        if (sealed)
            return FrameError{FrameErrorCode::INVALID_STATE};
        if (!descriptor.size || !descriptor.usage)
            return FrameError{FrameErrorCode::INVALID_ARGUMENT};
        if (descriptor.memory != MemoryType::GPU_ONLY)
            return FrameError{FrameErrorCode::UNSUPPORTED};
        if (data.buffers.size() >= 256)
            return FrameError{FrameErrorCode::OUT_OF_MEMORY};
        if (!id)
            id = FrameGraphIds::Get()->Next();
        if (!id)
            return FrameError{FrameErrorCode::INVALID_STATE};
#if SKY_ENABLE_RESOURCE_NAME
        descriptor.name = nullptr;
#endif
        FrameGraphBuffer buffer;
        buffer.size      = descriptor.size;
        buffer.transient = descriptor;
        const GraphBufferHandle handle{id, static_cast<uint32_t>(data.buffers.size())};
        data.buffers.push_back(std::move(buffer));
        return handle;
    }
    std::variant<FrameError, GraphImageHandle> FrameGraphBuilder::CreateTransient(Image::Descriptor descriptor) noexcept
    {
        if (sealed)
            return FrameError{FrameErrorCode::INVALID_STATE};
        if (!descriptor.extent.width || !descriptor.extent.height || !descriptor.mipLevels || !descriptor.arrayLayers || !descriptor.usage)
            return FrameError{FrameErrorCode::INVALID_ARGUMENT};
        if (descriptor.memory != MemoryType::GPU_ONLY || descriptor.usage.TestBit(ImageUsageFlagBit::TRANSIENT))
            return FrameError{FrameErrorCode::UNSUPPORTED};
        if (data.images.size() >= 256)
            return FrameError{FrameErrorCode::OUT_OF_MEMORY};
        if (!id)
            id = FrameGraphIds::Get()->Next();
        if (!id)
            return FrameError{FrameErrorCode::INVALID_STATE};
#if SKY_ENABLE_RESOURCE_NAME
        descriptor.name = nullptr;
#endif
        FrameGraphImage image;
        image.descriptor = descriptor;
        image.final      = ImageLayout::GENERAL;
        image.transient  = true;
        const GraphImageHandle handle{id, static_cast<uint32_t>(data.images.size())};
        data.images.push_back(std::move(image));
        return handle;
    }
    std::variant<FrameError, GraphBufferHandle>
    FrameGraphBuilder::Import(BufferPtr buffer, CopyBufferState initial, CopyBufferState final, bool initialized) noexcept
    {
        const auto size = buffer ? buffer->GetSize() : 0;
        return Import(FrameGraphBuffer{std::move(buffer), size, initial, final, initialized});
    }
    std::variant<FrameError, GraphBufferHandle> FrameGraphBuilder::Import(FrameGraphBuffer buffer) noexcept
    {
        if (sealed)
            return FrameError{FrameErrorCode::INVALID_STATE};
        if (!buffer.buffer || buffer.transient || !buffer.size || buffer.size != buffer.buffer->GetSize() ||
            !IsValidFrameBufferState(buffer.initial) || !IsValidFrameBufferState(buffer.final))
            return FrameError{FrameErrorCode::INVALID_ARGUMENT};
        std::vector<BufferContentRange> contents;
        if (!buffer.initializedRanges.empty()) {
            auto resolved = ResolveInitializedBufferContents(buffer);
            if (std::holds_alternative<FrameError>(resolved))
                return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Invalid initialized buffer contents"};
            contents = std::move(std::get<std::vector<BufferContentRange>>(resolved));
        }

        if (!id)
            id = FrameGraphIds::Get()->Next();
        if (!id)
            return FrameError{FrameErrorCode::INVALID_STATE};
        for (uint32_t i = 0; i < data.buffers.size(); ++i) {
            const auto &existing = data.buffers[i];
            if (existing.buffer.Get() != buffer.buffer.Get())
                continue;
            if (existing.size != buffer.size || existing.initial != buffer.initial || existing.final != buffer.final)
                return FrameError{FrameErrorCode::INVALID_ARGUMENT};
            if (existing.initializedRanges.empty() && buffer.initializedRanges.empty()) {
                if (existing.initialized != buffer.initialized)
                    return FrameError{FrameErrorCode::INVALID_ARGUMENT};
            } else {
                if (buffer.initialized)
                    contents.push_back({0, buffer.size});
                if (std::get<std::vector<BufferContentRange>>(ResolveInitializedBufferContents(existing)) != contents)
                    return FrameError{FrameErrorCode::INVALID_ARGUMENT};
            }
            return GraphBufferHandle{id, i};
        }
        if (data.buffers.size() >= 256)
            return FrameError{FrameErrorCode::OUT_OF_MEMORY};
        data.buffers.push_back(std::move(buffer));
        return GraphBufferHandle{id, static_cast<uint32_t>(data.buffers.size() - 1)};
    }
    std::variant<FrameError, GraphImageHandle> FrameGraphBuilder::Import(FrameGraphImage image) noexcept
    {
        if (sealed)
            return FrameError{FrameErrorCode::INVALID_STATE};
        if (!image.image || image.transient)
            return FrameError{FrameErrorCode::INVALID_ARGUMENT};
        const auto contents = ResolveInitializedImageContents(image);
        if (std::holds_alternative<FrameError>(contents))
            return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Invalid initialized image contents"};

        if (!id)
            id = FrameGraphIds::Get()->Next();
        if (!id)
            return FrameError{FrameErrorCode::INVALID_STATE};
        for (uint32_t i = 0; i < data.images.size(); ++i) {
            const auto &existing = data.images[i];
            if (existing.image.Get() != image.image.Get())
                continue;
            if (!existing.image.HasSameLifetime(image.image) || !IsSameImageStorage(existing.descriptor, image.descriptor) ||
                existing.initial != image.initial || existing.final != image.final ||
                std::get<ResolvedImageContents>(ResolveInitializedImageContents(existing)).ranges != std::get<ResolvedImageContents>(contents).ranges)
                return FrameError{FrameErrorCode::INVALID_ARGUMENT};
            return GraphImageHandle{id, i};
        }
        if (data.images.size() >= 256)
            return FrameError{FrameErrorCode::OUT_OF_MEMORY};
#if SKY_ENABLE_RESOURCE_NAME
        image.descriptor.name = nullptr;
#endif
        data.images.push_back(std::move(image));
        return GraphImageHandle{id, static_cast<uint32_t>(data.images.size() - 1)};
    }
    FrameOperationResult FrameGraphBuilder::Copy(BufferCopyPass pass) noexcept
    {
        return Add(pass);
    }
    FrameOperationResult FrameGraphBuilder::Copy(BufferImageCopyPass pass) noexcept
    {
        return Add(pass);
    }
    FrameOperationResult FrameGraphBuilder::Raster(FrameRasterPass pass) noexcept
    {
        return Add(std::move(pass));
    }
    FrameOperationResult FrameGraphBuilder::Fullscreen(FrameRasterPass pass) noexcept
    {
        if (pass.draws.size() != 1)
            return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Fullscreen requires one draw"};
        for (const auto &draw : pass.draws)
            if (!draw.vertices.empty() || draw.indexed || draw.command.vertexCount != 3 || draw.command.instanceCount != 1 ||
                draw.command.firstVertex || draw.command.firstInstance)
                return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Fullscreen requires a generated triangle"};
        return Raster(std::move(pass));
    }
    FrameOperationResult FrameGraphBuilder::Compute(FrameComputePass pass) noexcept
    {
        return Add(std::move(pass));
    }
    FrameOperationResult FrameGraphBuilder::Custom(FrameCustomPass pass) noexcept
    {
        return Add(std::move(pass));
    }
    FrameOperationResult FrameGraphBuilder::Reset() noexcept
    {
        auto result = arena.Reset();
        if (std::holds_alternative<FrameError>(result))
            return result;
        data   = {};
        id     = 0;
        sealed = false;
        return FrameOperationComplete{};
    }
    FrameOperationResult FrameGraphBuilder::Add(FrameGraphPass pass) noexcept
    {
        if (sealed)
            return FrameError{FrameErrorCode::INVALID_STATE};
        if (data.passes.size() >= 1024)
            return FrameError{FrameErrorCode::OUT_OF_MEMORY};

        data.passes.push_back(std::move(pass));
        return FrameOperationComplete{};
    }
    FrameOperationResult FrameGraphBuilder::Export(GraphBufferExport output) noexcept
    {
        if (sealed)
            return FrameError{FrameErrorCode::INVALID_STATE};
        if (data.exports.size() >= 256)
            return FrameError{FrameErrorCode::OUT_OF_MEMORY};

        data.exports.push_back(output);
        return FrameOperationComplete{};
    }
    FrameOperationResult FrameGraphBuilder::KeepAlive(std::shared_ptr<const void> payload) noexcept
    {
        if (sealed)
            return FrameError{FrameErrorCode::INVALID_STATE};
        data.payload = std::move(payload);
        return FrameOperationComplete{};
    }
    FrameOperationResult FrameGraphBuilder::Export(GraphImageHandle output) noexcept
    {
        if (sealed)
            return FrameError{FrameErrorCode::INVALID_STATE};
        if (data.imageExports.size() >= 256)
            return FrameError{FrameErrorCode::OUT_OF_MEMORY};

        data.imageExports.push_back(output);
        return FrameOperationComplete{};
    }
    std::variant<FrameError, std::shared_ptr<const FrozenFrameGraph>> FrameGraphBuilder::Freeze() noexcept
    {
        if (sealed)
            return FrameError{FrameErrorCode::INVALID_STATE};

        if (!id)
            id = FrameGraphIds::Get()->Next();
        if (!id)
            return FrameError{FrameErrorCode::INVALID_STATE};
        auto allocated = arena.Make<FrozenFrameGraph>(id, std::move(data));
        if (auto *error = std::get_if<FrameError>(&allocated))
            return std::move(*error);
        std::shared_ptr<const FrozenFrameGraph> graph(arena.Seal(), std::get<FrozenFrameGraph *>(allocated));
        sealed = true;
        return graph;
    }
} // namespace sky::aurora

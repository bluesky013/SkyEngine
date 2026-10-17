#pragma once

#include "GraphBuildArena.h"
#include <aurora/rdg/FrozenFrameGraph.h>

namespace sky::aurora {

    class FrameGraphBuilder {
    public:
        FrameGraphBuilder() = default;
        uint64_t GetId() const noexcept
        {
            return id;
        }
        uint32_t GetPassCount() const noexcept
        {
            return static_cast<uint32_t>(data.passes.size());
        }
        FrameGraphBuilder(const FrameGraphBuilder &)                                     = delete;
        FrameGraphBuilder                          &operator=(const FrameGraphBuilder &) = delete;
        std::variant<FrameError, GraphBufferHandle> Import(BufferPtr, CopyBufferState initial, CopyBufferState final, bool initialized) noexcept;
        std::variant<FrameError, GraphBufferHandle> Import(FrameGraphBuffer buffer) noexcept;
        std::variant<FrameError, GraphImageHandle>  Import(FrameGraphImage image) noexcept;
        std::variant<FrameError, GraphBufferHandle> CreateTransient(Buffer::Descriptor descriptor) noexcept;
        std::variant<FrameError, GraphImageHandle>  CreateTransient(Image::Descriptor descriptor) noexcept;
        FrameOperationResult                        Copy(BufferCopyPass pass) noexcept;
        FrameOperationResult                        Copy(BufferImageCopyPass pass) noexcept;
        FrameOperationResult                        Raster(FrameRasterPass pass) noexcept;
        FrameOperationResult                        Fullscreen(FrameRasterPass pass) noexcept;
        FrameOperationResult                        Compute(FrameComputePass pass) noexcept;
        FrameOperationResult                        Custom(FrameCustomPass pass) noexcept;
        FrameOperationResult                        Reset() noexcept;
        FrameOperationResult                        Export(GraphBufferExport output) noexcept;
        FrameOperationResult                        Export(GraphImageHandle output) noexcept;
        FrameOperationResult                        KeepAlive(std::shared_ptr<const void> payload) noexcept;
        std::variant<FrameError, std::shared_ptr<const FrozenFrameGraph>> Freeze() noexcept;

    private:
        FrameOperationResult Add(FrameGraphPass pass) noexcept;
        uint64_t             id     = 0;
        bool                 sealed = false;
        FrameGraphData       data;
        GraphBuildArena      arena{sizeof(FrozenFrameGraph) + alignof(FrozenFrameGraph)};
    };
} // namespace sky::aurora

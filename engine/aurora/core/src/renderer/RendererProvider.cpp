#include "RendererRuntime.h"
#include <aurora/renderer/RendererProvider.h>

namespace sky::aurora {

    class RendererProvider final : public IRendererProvider {
    public:
        RendererCreateResult Create(const RendererInitInfo &info) override
        {
            if (info.inflightFrames == 0 || info.maxPendingPackets == 0 || info.prepareWorkers == 0 || info.encodeWorkers == 0 ||
                info.maxPendingResourceBytes == 0 || info.maxUploadBytesPerFrame < 4) {
                return RendererError{RendererErrorCode::INVALID_ARGUMENT, "Frame, queue, worker counts and resource budgets must be nonzero"};
            }
            if (info.maxPendingPackets > 64 || info.inflightFrames > 8 || info.prepareWorkers > 64 || info.encodeWorkers > 64 ||
                info.maxUploadBytesPerFrame > UINT64_MAX / info.inflightFrames || info.vulkanMode > RendererVulkanMode::RENDER_PASS ||
                info.backend > API::DX12)
                return RendererError{RendererErrorCode::INVALID_ARGUMENT, "Renderer configuration exceeds supported ranges"};
            return CreateRuntimeRenderer(info);
        }
    };

    std::unique_ptr<IRendererProvider> CreateRendererProvider()
    {
        return std::make_unique<RendererProvider>();
    }

} // namespace sky::aurora

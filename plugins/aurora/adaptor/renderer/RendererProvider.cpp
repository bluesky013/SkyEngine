#include "RendererProvider.h"

namespace sky::aurora {

    class RendererProvider final : public IRendererProvider {
    public:
        RendererCreateResult Create(const RendererInitInfo &info) override
        {
            if (info.inflightFrames == 0 || info.maxPendingPackets == 0 || info.prepareWorkers == 0 || info.encodeWorkers == 0 ||
                info.maxPendingResourceBytes == 0 || info.maxUploadBytesPerFrame == 0) {
                return RendererError{RendererErrorCode::INVALID_ARGUMENT, "Frame, queue, worker counts and resource budgets must be nonzero"};
            }
            return RendererError{RendererErrorCode::NOT_READY, "Aurora Renderer M0 provides contracts only; runtime creation is not implemented"};
        }
    };

    std::unique_ptr<IRendererProvider> CreateRendererProvider()
    {
        return std::make_unique<RendererProvider>();
    }

} // namespace sky::aurora

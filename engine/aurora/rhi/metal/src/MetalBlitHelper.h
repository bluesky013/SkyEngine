//
// Metal blit/resolve helper.
//
// MTLBlitCommandEncoder cannot scale or resolve images, so filtered blits go
// through a lazily-built fullscreen-triangle pipeline and explicit resolves
// through a store-action resolve render pass. Owned by MetalDevice; the blit
// encoder suspends its MTLBlitCommandEncoder around these render passes.
//

#pragma once

#include <aurora/rhi/Core.h>

#include <unordered_map>
#include <vector>

namespace sky::aurora {

    class MetalDevice;
    class MetalImage;

    class MetalBlitHelper {
    public:
        explicit MetalBlitHelper(MetalDevice &dev);
        ~MetalBlitHelper();

        MetalBlitHelper(const MetalBlitHelper &)            = delete;
        MetalBlitHelper &operator=(const MetalBlitHelper &) = delete;

        // render-based filtered blit; commandBuffer is id<MTLCommandBuffer>.
        // Returns false when the dst format cannot be rendered into.
        bool Blit(void *commandBuffer, MetalImage *src, MetalImage *dst, const std::vector<BlitInfo> &regions, Filter filter);

        // resolve pass per region (MSAA src -> single-sampled dst via store action)
        void Resolve(void *commandBuffer, MetalImage *src, MetalImage *dst, const std::vector<ResolveInfo> &regions);

    private:
        // mtlFormat is MTLPixelFormat kept as uint32_t to hide Obj-C types
        void *GetBlitPipeline(uint32_t mtlFormat, uint32_t sampleCount);
        void *GetSampler(bool linear);

        MetalDevice                         &device;
        std::unordered_map<uint64_t, void *> pipelines;                // id<MTLRenderPipelineState> by (format, samples)
        void                                *linearSampler  = nullptr; // id<MTLSamplerState>
        void                                *nearestSampler = nullptr; // id<MTLSamplerState>
    };

} // namespace sky::aurora

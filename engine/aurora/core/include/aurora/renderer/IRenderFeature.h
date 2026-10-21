#pragma once

#include <aurora/renderer/PreparedFrameTypes.h>
#include <aurora/rhi/SwapChain.h>
#include <span>

namespace sky::aurora {

    class PreparePlanBuilder;
    struct FrameRenderTarget {
        FrameOutputInfo  output;
        GraphImageHandle color;
        uint64_t         revision      = 0;
        SurfaceTransform transform     = SurfaceTransform::IDENTITY;
        bool             initialized   = false;
        Matrix4          logicalToClip = RenderIdentityMatrix();
    };

    // Coordinator-only borrowed context. It exposes declarations and lease allocations, never submission.
    class FrameGraphBuildContext {
    public:
        virtual ~FrameGraphBuildContext()                                                                        = default;
        virtual const FrameBuildInputs                     &GetInputs() const                                    = 0;
        virtual std::optional<FrameRenderTarget>            GetOutput(ViewportId) const                          = 0;
        virtual std::optional<GraphImageHandle>             ResolveViewOutput(ViewOutputRef) const               = 0;
        virtual std::optional<GraphImageHandle>             ResolveImage(const RenderResourceKey &) const        = 0;
        virtual std::variant<FrameError, ShaderPtr>         CreateShader(const RenderMaterialData &)             = 0;
        virtual std::variant<FrameError, SamplerPtr>        CreateSampler(const Sampler::Descriptor &)           = 0;
        virtual std::variant<FrameError, RasterBufferRange> Upload(std::span<const std::byte>, BufferUsageFlags) = 0;
        virtual std::variant<FrameError, RasterUniformBinding>
        WriteUniform(uint32_t set, uint32_t binding, std::span<const std::byte>, ShaderResourceType type = ShaderResourceType::UNIFORM_BUFFER) = 0;
        // Resolves only the current BuildView snapshot, retaining its exact resource versions.
        virtual std::variant<FrameError, std::vector<ResolvedRasterDraw>> ResolveDraws(Rect2D           scissor,
                                                                                       PreparedDrawList list = PreparedDrawList::VISIBLE) = 0;
        virtual std::variant<FrameError, GraphImageHandle>                CreateImage(const Image::Descriptor &)                          = 0;
        virtual FrameOperationResult                                      Raster(FrameRasterPass)                                         = 0;
        virtual FrameOperationResult                                      Fullscreen(FrameRasterPass)                                     = 0;
    };

    class IScenePrepareFeature {
    public:
        virtual ~IScenePrepareFeature()                                = default;
        virtual Name GetName() const                                   = 0;
        virtual void RegisterScenePrepare(PreparePlanBuilder &builder) = 0;
        virtual void RegisterViewPrepare(PreparePlanBuilder &builder)  = 0;
    };

    // Registration/removal is coordinator-owned; unload waits for all plan CPU/GPU borrowers.
    class IFrameGraphFeature {
    public:
        virtual ~IFrameGraphFeature()                        = default;
        virtual Name                         GetName() const = 0;
        virtual std::optional<RendererError> Validate(const FrameExtensionPayload &) const
        {
            return {};
        }
        // Coordinator-only; context borrows expire on return. No queue submission or live ECS mutation.
        virtual FrameOperationResult BuildView(FrameGraphBuildContext &, const PreparedViewData &)
        {
            return FrameOperationComplete{};
        }
        virtual FrameOperationResult BuildFrame(FrameGraphBuildContext &, const FrameExtensionPayload &) = 0;
        // Runs on the coordinator after the final CPU/GPU/presentation borrower; release native caches here.
        virtual void OnRetired()
        {
        }
    };

} // namespace sky::aurora

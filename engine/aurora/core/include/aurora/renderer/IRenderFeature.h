#pragma once

#include <aurora/rdg/FrameBindings.h>
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

    class FrameGraphDeclarations {
    public:
        virtual ~FrameGraphDeclarations()                                                            = default;
        virtual std::variant<FrameError, GraphBufferHandle> CreateBuffer(const Buffer::Descriptor &) = 0;
        virtual std::variant<FrameError, GraphImageHandle>  CreateImage(const Image::Descriptor &)   = 0;
        virtual FrameOperationResult                        Copy(BufferCopyPass)                     = 0;
        virtual FrameOperationResult                        Copy(BufferImageCopyPass)                = 0;
        virtual FrameOperationResult                        Raster(FrameRasterPass)                  = 0;
        virtual FrameOperationResult                        Fullscreen(FrameRasterPass)              = 0;
        virtual FrameOperationResult                        Compute(FrameComputePass)                = 0;
        virtual FrameOperationResult                        Custom(FrameCustomPass)                  = 0;
        virtual FrameOperationResult                        Export(GraphBufferExport)                = 0;
        virtual FrameOperationResult                        Export(GraphImageHandle)                 = 0;
    };

    // Resource resolution borrows the current prepared snapshot; it never exposes submission.
    class FrameGraphServices {
    public:
        virtual ~FrameGraphServices()                                                                            = default;
        virtual const FrameBuildInputs                     &GetInputs() const                                    = 0;
        virtual std::optional<FrameRenderTarget>            GetOutput(ViewportId) const                          = 0;
        virtual std::optional<GraphImageHandle>             ResolveViewOutput(ViewOutputRef) const               = 0;
        virtual std::optional<GraphImageHandle>             ResolveImage(const RenderResourceKey &) const        = 0;
        virtual std::variant<FrameError, ShaderPtr>         CreateShader(const RenderTechniqueData &)            = 0;
        virtual std::variant<FrameError, SamplerPtr>        CreateSampler(const Sampler::Descriptor &)           = 0;
        virtual std::variant<FrameError, RasterBufferRange> Upload(std::span<const std::byte>, BufferUsageFlags) = 0;
        virtual std::variant<FrameError, RasterUniformBinding>
        WriteUniform(uint32_t set, uint32_t binding, std::span<const std::byte>, ShaderResourceType type = ShaderResourceType::UNIFORM_BUFFER) = 0;
        // Completes one raster parameter set; graph ranges still require Compile validation.
        virtual std::variant<FrameError, FrameBindingGroupUse>
        MaterializeParameters(ShaderPtr, uint32_t set, const ShaderParameterTransactionState &, RasterBindingDraft resources);
        // Resolves only the current BuildView snapshot, retaining its exact resource versions.
        virtual std::variant<FrameError, std::vector<ResolvedRasterDraw>>
        ResolveDraws(Name tag, Rect2D scissor, PreparedDrawList list = PreparedDrawList::VISIBLE, const ShaderVariant *pipelineValues = nullptr) = 0;

    protected:
        static std::optional<FrameError>
        ValidateParameterResources(ShaderPtr, uint32_t, const ShaderParameterTransactionState &, const RasterBindingDraft &);
    };

    // Coordinator-only facets; all borrows expire when the feature callback returns.
    class FrameGraphBuildContext : public FrameGraphDeclarations, public FrameGraphServices {
    public:
        ~FrameGraphBuildContext() override = default;
    };

    // Coordinator preparation precedes graph declarations and output acquisition.
    class FrameFeaturePrepareContext {
    public:
        virtual ~FrameFeaturePrepareContext()                                                 = default;
        virtual RendererInfo                        GetRendererInfo() const                   = 0;
        virtual const FrameBuildInputs             &GetInputs() const                         = 0;
        virtual std::variant<FrameError, ShaderPtr> CreateShader(const RenderTechniqueData &) = 0;
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
        virtual FrameOperationResult Prepare(FrameFeaturePrepareContext &)
        {
            return FrameOperationComplete{};
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

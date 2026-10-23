#pragma once

#include <aurora/rdg/FrameExecutionTypes.h>
#include <aurora/rhi/Buffer.h>
#include <aurora/rhi/Image.h>
#include <aurora/rhi/Sampler.h>
#include <aurora/rhi/Shader.h>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace sky::aurora {

    class CommandBuffer;

    // Values are append-only: existing copy/raster validation ranges stay stable.
    enum class CopyBufferState : uint8_t { COMMON, HOST_WRITE, COPY_SOURCE, COPY_DESTINATION, HOST_READ, RASTER_READ, SHADER_READ, UNORDERED_ACCESS };
    constexpr bool IsValidFrameBufferState(CopyBufferState state) noexcept
    {
        return state <= CopyBufferState::UNORDERED_ACCESS;
    }

    struct GraphBufferHandle {
        uint64_t graph                                       = 0;
        uint32_t index                                       = UINT32_MAX;
        bool     operator==(const GraphBufferHandle &) const = default;
    };

    struct BufferContentRange {
        uint64_t offset = 0, size = 0;
        bool     operator==(const BufferContentRange &) const = default;
    };
    struct FrameGraphBuffer {
        BufferPtr                         buffer;
        uint64_t                          size        = 0;
        CopyBufferState                   initial     = CopyBufferState::COMMON;
        CopyBufferState                   final       = CopyBufferState::COMMON;
        bool                              initialized = false;
        std::optional<Buffer::Descriptor> transient;
        // Byte coverage only; partial declarations exclude whole initialization and transient storage.
        std::vector<BufferContentRange> initializedRanges;
    };

    struct BufferCopyPass {
        GraphBufferHandle source;
        GraphBufferHandle destination;
        uint64_t          sourceOffset      = 0;
        uint64_t          destinationOffset = 0;
        uint64_t          size              = 0;
        bool              sideEffect        = false;
    };

    struct GraphBufferExport {
        GraphBufferHandle buffer;
        uint64_t          offset = 0;
        uint64_t          size   = 0;
    };

    struct GraphImageHandle {
        uint64_t graph                                      = 0;
        uint32_t index                                      = UINT32_MAX;
        bool     operator==(const GraphImageHandle &) const = default;
    };
    struct ImageContentRegion {
        uint32_t mip = 0, layer = 0;
        Rect2D   rectangle{};
    };
    struct ImageBoundaryState {
        ImageBoundaryState(ImageLayout layout = ImageLayout::UNDEFINED) : layouts(layout)
        {
        }
        explicit ImageBoundaryState(std::vector<ImageLayout> values) : layouts(std::move(values))
        {
        }
        // Validate against the descriptor before indexing; explicit tables use layer * mipLevels + mip.
        ImageLayout At(uint32_t subresource) const
        {
            const auto *uniform = std::get_if<ImageLayout>(&layouts);
            return uniform ? *uniform : std::get<std::vector<ImageLayout>>(layouts)[subresource];
        }
        bool                                                operator==(const ImageBoundaryState &) const noexcept;
        std::variant<ImageLayout, std::vector<ImageLayout>> layouts;
    };
    std::optional<FrameError> ValidateImageBoundaryState(const ImageBoundaryState &, uint64_t subresourceCount, bool final = false) noexcept;

    class FrameImageReference {
    public:
        FrameImageReference() = default;
        FrameImageReference(ImagePtr value) : owned(std::move(value))
        {
        }
        static FrameImageReference Borrow(Image *image, DeviceFrameLease lease, std::shared_ptr<const void> owner)
        {
            FrameImageReference result;
            result.borrowed = image;
            result.lease    = lease;
            result.owner    = std::move(owner);
            return result;
        }
        Image *Get() const noexcept
        {
            return owned ? owned.Get() : borrowed;
        }
        Image *operator->() const noexcept
        {
            return Get();
        }
        explicit operator bool() const noexcept
        {
            return Get() != nullptr;
        }
        bool IsBorrowed() const noexcept
        {
            return borrowed != nullptr;
        }
        bool ValidFor(const DeviceFrameLease &value) const noexcept
        {
            return !borrowed || (owner && lease == value);
        }
        bool HasOwner() const noexcept
        {
            return !borrowed || bool(owner);
        }
        const ImagePtr &GetOwned() const noexcept
        {
            return owned;
        }
        bool HasSameLifetime(const FrameImageReference &other) const noexcept
        {
            return Get() == other.Get() && IsBorrowed() == other.IsBorrowed() && (!borrowed || (lease == other.lease && owner == other.owner));
        }

    private:
        ImagePtr                    owned;
        Image                      *borrowed = nullptr;
        DeviceFrameLease            lease;
        std::shared_ptr<const void> owner;
    };

    struct FrameGraphImage {
        FrameImageReference image;
        Image::Descriptor   descriptor;
        ImageBoundaryState  initial = ImageLayout::UNDEFINED;
        ImageBoundaryState  final   = ImageLayout::TRANSFER_SRC;
        // Whole initialization excludes partial declarations; contents do not change the boundary layout.
        bool                            initialized = false;
        bool                            transient   = false;
        std::vector<ImageSubRange>      initializedRanges;
        std::vector<ImageContentRegion> initializedRegions;
    };
    enum class ImageCopyDirection : uint8_t { BUFFER_TO_IMAGE, IMAGE_TO_BUFFER };
    struct BufferImageCopyPass {
        GraphBufferHandle  buffer;
        GraphImageHandle   image;
        BufferImageCopy    region;
        ImageCopyDirection direction  = ImageCopyDirection::BUFFER_TO_IMAGE;
        bool               sideEffect = false;
    };
    struct RasterBufferRange {
        GraphBufferHandle buffer;
        uint64_t          offset = 0;
        uint64_t          size   = 0;
    };
    struct RasterVertexStream {
        RasterBufferRange range;
        uint32_t          stride = 0;
        VertexInputRate   rate   = VertexInputRate::PER_VERTEX;
    };
    struct RasterVertexAttribute {
        uint32_t    location = 0, binding = 0, offset = 0;
        Format      format = Format::UNDEFINED;
        std::string semantic;
        uint32_t    semanticIndex                                   = 0;
        bool        operator==(const RasterVertexAttribute &) const = default;
    };
    struct RasterIndexedDraw {
        RasterBufferRange range;
        IndexType         type = IndexType::U32;
        CmdDrawIndexed    command;
    };
    struct RasterUniformBinding {
        uint32_t          set = 0, binding = 0;
        RasterBufferRange range;
        uint32_t          dynamicOffset = 0;
    };
    struct RasterDepthState {
        bool      test = false, write = false;
        CompareOp compare                                    = CompareOp::LESS_OR_EQUAL;
        bool      operator==(const RasterDepthState &) const = default;
    };
    struct RasterImageView {
        ImageViewType type    = ImageViewType::VIEW_2D;
        uint32_t      baseMip = 0, mipCount = 0;
        uint32_t      baseLayer = 0, layerCount = 1;
        // Zero mipCount selects the remaining mips; layers always have an explicit nonzero count.
        bool operator==(const RasterImageView &) const = default;
    };
    struct RasterImageBinding {
        uint32_t         set = 0, binding = 0;
        GraphImageHandle image;
        RasterImageView  view;
    };
    struct RasterSamplerBinding {
        uint32_t   set = 0, binding = 0;
        SamplerPtr sampler;
    };
    struct RasterDraw {
        ShaderPtr                          shader;
        BlendState                         blend;
        CmdDrawLinear                      command;
        Rect2D                             scissor{};
        std::vector<RasterVertexStream>    vertices;
        std::vector<RasterVertexAttribute> attributes;
        std::optional<RasterIndexedDraw>   indexed;
        std::vector<RasterUniformBinding>  uniforms;
        std::vector<RasterImageBinding>    images;
        std::vector<RasterSamplerBinding>  samplers;
        RasterDepthState                   depth;
    };
    // One mip and one array layer; attachment views do not span subresources.
    struct RasterAttachmentView {
        uint32_t mip = 0, layer = 0;
        bool     operator==(const RasterAttachmentView &) const = default;
    };
    struct ResolvedRasterAttachmentView {
        ImageSubRange range;
        Extent2D      extent;
        uint32_t      subresource = 0;
    };
    enum class DepthResolveMode : uint8_t { UNDEFINED, MINIMUM, MAXIMUM };
    struct RasterDepthResolve {
        GraphImageHandle     image;
        RasterAttachmentView view;
        DepthResolveMode     mode = DepthResolveMode::UNDEFINED;
    };
    struct RasterDepthAttachment {
        GraphImageHandle                  image;
        LoadOp                            load  = LoadOp::CLEAR;
        StoreOp                           store = StoreOp::STORE;
        float                             clear = 1.f;
        RasterAttachmentView              view;
        std::optional<RasterDepthResolve> resolve;
    };
    struct RasterColorResolve {
        GraphImageHandle     image;
        RasterAttachmentView view;
    };
    struct FrameRasterPass {
        // A default color handle declares a depth-only pass; depth is then required.
        GraphImageHandle        color;
        LoadOp                  load  = LoadOp::CLEAR;
        StoreOp                 store = StoreOp::STORE;
        ClearValue              clear{0.f, 0.f, 0.f, 1.f};
        std::vector<RasterDraw> draws;
        bool                    sideEffect = false;
        // Shader reads the loaded color attachment; ordinary LOAD/blending does not request local read.
        bool                                 colorLocalRead = false;
        std::optional<RasterDepthAttachment> depth;
        RasterAttachmentView                 colorView;
        // Resolve preserves its result even when the multisampled color is discarded.
        std::optional<RasterColorResolve> colorResolve;
    };
    enum class FrameAccessMode : uint8_t { READ, WRITE, READ_WRITE };
    // REFLECTED is valid only for Compute; Custom declares the descriptor kind explicitly.
    enum class FrameBufferBindingKind : uint8_t { REFLECTED, UNIFORM, STORAGE };
    enum class FrameImageBindingKind : uint8_t { REFLECTED, SAMPLED, STORAGE };

    struct FrameBufferBinding {
        uint32_t               set = 0, binding = 0;
        RasterBufferRange      range;
        uint32_t               dynamicOffset  = 0;
        FrameAccessMode        access         = FrameAccessMode::READ;
        FrameBufferBindingKind kind           = FrameBufferBindingKind::REFLECTED;
        ShaderResourceAccess   resourceAccess = ShaderResourceAccess::READ_WRITE;
        ShaderBufferKind       bufferKind     = ShaderBufferKind::RAW;
        uint32_t               elementStride  = 0;
    };
    struct FrameImageBinding {
        uint32_t              set = 0, binding = 0;
        GraphImageHandle      image;
        RasterImageView       view;
        FrameAccessMode       access = FrameAccessMode::READ;
        FrameImageBindingKind kind   = FrameImageBindingKind::REFLECTED;
    };
    // Declarations retain their order; native bindings are identified by set/binding.
    struct FrameComputePass {
        ShaderPtr shader;
        // Zero dimensions are rejected: a no-op cannot define declared write contents.
        uint32_t                          groupsX = 1, groupsY = 1, groupsZ = 1;
        std::vector<FrameBufferBinding>   buffers;
        std::vector<FrameImageBinding>    images;
        std::vector<RasterSamplerBinding> samplers;
        bool                              sideEffect = false;
    };

    class FrameCustomPassEncoder;
    struct FrameCustomPassContext {
        CommandBuffer           &command;
        std::span<Image *const>  images;  // Same order as FrameCustomPass::images.
        std::span<Buffer *const> buffers; // Same order as FrameCustomPass::buffers.
    };
    class FrameCustomPassEncoder {
    public:
        virtual ~FrameCustomPassEncoder() = default;
        // Declared accesses are already synchronized; the callback must not issue barriers.
        // Finish all encoders before returning; command/context references must not escape the callback.
        virtual FrameOperationResult Encode(FrameCustomPassContext &) const noexcept = 0;
    };
    struct FrameCustomPass {
        std::string                                   name;
        std::shared_ptr<const FrameCustomPassEncoder> encoder;
        std::vector<FrameBufferBinding>               buffers;
        std::vector<FrameImageBinding>                images;
        bool                                          sideEffect = false;
    };
    using FrameGraphPass = std::variant<BufferCopyPass, BufferImageCopyPass, FrameRasterPass, FrameComputePass, FrameCustomPass>;

    struct FrameGraphData {
        std::vector<FrameGraphBuffer>  buffers;
        std::vector<FrameGraphImage>   images;
        std::vector<FrameGraphPass>    passes;
        std::vector<GraphBufferExport> exports;
        std::vector<GraphImageHandle>  imageExports;
        std::shared_ptr<const void>    payload;
    };

    // Immutable declarations and retained objects; buffer contents still obey external access/retirement rules.
    class FrozenFrameGraph final {
    public:
        FrozenFrameGraph(uint64_t id, FrameGraphData data) : id(id), data(std::move(data))
        {
        }
        uint64_t GetId() const noexcept
        {
            return id;
        }
        const FrameGraphData &GetData() const noexcept
        {
            return data;
        }

    private:
        const uint64_t       id;
        const FrameGraphData data;
    };

    struct FrameResourceLifetime {
        // Inclusive logical pass indices; exports extend through the graph-end sentinel.
        uint32_t first = UINT32_MAX, last = 0;
    };
    struct FrameGraphAnalysis {
        std::vector<uint32_t>              livePasses;
        std::vector<uint32_t>              usedBuffers;
        std::vector<uint32_t>              usedImages;
        std::vector<FrameResourceLifetime> bufferLifetimes;
        std::vector<FrameResourceLifetime> imageLifetimes;
    };
    using FrameGraphAnalysisResult = std::variant<FrameError, FrameGraphAnalysis>;

    // Effective byte ranges include dynamic offsets; storage descriptors expose exactly one mip.
    std::variant<FrameError, RasterBufferRange> ResolveFrameBufferRange(const FrameBufferBinding &, uint64_t bufferSize) noexcept;
    std::variant<FrameError, RasterImageView>
    ResolveFrameImageView(const FrameImageBinding &, const Image::Descriptor &, bool storage = false) noexcept;
    // Requires validated compute bindings; returns dynamic offsets in ascending binding order.
    std::vector<uint32_t> ResolveComputeDynamicOffsets(const FrameComputePass &, uint32_t set);

    constexpr uint32_t                                        MAX_BUFFER_INITIAL_RANGES = 256;
    std::variant<FrameError, std::vector<BufferContentRange>> ResolveInitializedBufferContents(const FrameGraphBuffer &) noexcept;

    constexpr uint32_t MAX_FRAME_IMAGE_SUBRESOURCES   = 65536;
    constexpr uint32_t MAX_FRAME_IMAGE_COPY_RANGES    = 65536;
    constexpr uint32_t MAX_FRAME_IMAGE_INITIAL_RANGES = 65536;
    struct ImageContentSpan {
        uint64_t begin = 0, end = 0;
        bool     operator==(const ImageContentSpan &) const = default;
    };
    struct ResolvedImageContents {
        // Texel addresses are layer-major, then mip-major; spans are sorted and merged.
        std::vector<uint64_t>         subresourceOffsets;
        std::vector<ImageContentSpan> ranges;
        uint32_t                      expandedRanges = 0;
    };
    std::variant<FrameError, ResolvedImageContents> ResolveInitializedImageContents(const FrameGraphImage &,
                                                                                    uint32_t rangeBudget = MAX_FRAME_IMAGE_INITIAL_RANGES) noexcept;

    // Declaration order is semantic order; this helper does not choose native barriers or encode units.
    FrameGraphAnalysisResult AnalyzeFrameGraph(const FrozenFrameGraph &graph) noexcept;

    std::variant<FrameError, ResolvedRasterAttachmentView> ResolveRasterAttachmentView(const RasterAttachmentView &,
                                                                                       const Image::Descriptor &) noexcept;
    std::variant<FrameError, RasterImageView>              ResolveRasterImageView(const RasterImageView &, const Image::Descriptor &) noexcept;
    bool                                                   IsRasterResourceSupported(const ShaderResource &) noexcept;
    std::optional<FrameError>                              ValidateRasterBindings(const RasterDraw &, const ShaderReflection &) noexcept;
    bool                                                   RasterPipelineLess(const RasterDraw &, const RasterDraw &) noexcept;
    PipelineState                                          RasterPipelineState(const RasterDraw &);

} // namespace sky::aurora

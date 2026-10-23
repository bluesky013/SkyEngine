#include <aurora/rdg/FrozenFrameGraph.h>

#include <algorithm>
#include <aurora/rdg/FrameBindingLookup.h>
#include <bit>
#include <cmath>
#include <tuple>

namespace sky::aurora {
    namespace {
        using Range = ImageContentSpan;
        bool ColorFormat(PixelFormat format)
        {
            return format == PixelFormat::RGBA8_UNORM || format == PixelFormat::RGBA8_SRGB || format == PixelFormat::BGRA8_UNORM ||
                   format == PixelFormat::BGRA8_SRGB || format == PixelFormat::RGBA16_SFLOAT;
        }
        bool Overlaps(const std::vector<Range> &ranges, Range requested)
        {
            const auto found =
                std::lower_bound(ranges.begin(), ranges.end(), requested.begin, [](auto range, auto begin) { return range.end <= begin; });
            return found != ranges.end() && found->begin < requested.end;
        }
        void Include(std::vector<Range> &ranges, Range added)
        {
            auto first = std::lower_bound(ranges.begin(), ranges.end(), added.begin, [](auto range, auto begin) { return range.end < begin; });
            auto last  = first;
            while (last != ranges.end() && last->begin <= added.end) {
                added.begin = std::min(added.begin, last->begin);
                added.end   = std::max(added.end, last->end);
                ++last;
            }
            ranges.insert(ranges.erase(first, last), added);
        }
        void Remove(std::vector<Range> &ranges, Range written)
        {
            auto first = std::lower_bound(ranges.begin(), ranges.end(), written.begin, [](auto range, auto begin) { return range.end <= begin; });
            auto last  = first;
            while (last != ranges.end() && last->begin < written.end)
                ++last;
            if (first == last)
                return;
            const auto begin = first->begin, end = (last - 1)->end;
            auto       position = ranges.erase(first, last);
            if (end > written.end)
                position = ranges.insert(position, {written.end, end});
            if (begin < written.begin)
                ranges.insert(position, {begin, written.begin});
        }
        bool Covered(const std::vector<Range> &ranges, Range requested)
        {
            const auto found =
                std::lower_bound(ranges.begin(), ranges.end(), requested.begin, [](auto range, auto begin) { return range.end <= begin; });
            return found != ranges.end() && found->begin <= requested.begin && found->end >= requested.end;
        }
        FrameError Invalid(const char *reason)
        {
            return {FrameErrorCode::COMPILE_FAILED, reason};
        }
        std::variant<FrameError, RasterImageView> NormalizeImageView(RasterImageView view, const Image::Descriptor &image, const char *rangeError)
        {
            if (view.baseMip >= image.mipLevels || view.baseLayer >= image.arrayLayers || !view.layerCount ||
                view.layerCount > image.arrayLayers - view.baseLayer || view.mipCount > image.mipLevels - view.baseMip)
                return Invalid(rangeError);
            if (view.type == ImageViewType::VIEW_2D && (image.arrayLayers != 1 || view.layerCount != 1))
                return FrameError{FrameErrorCode::UNSUPPORTED, "Array images require an array view even for a single layer"};
            if (!view.mipCount)
                view.mipCount = image.mipLevels - view.baseMip;
            return view;
        }
    } // namespace

    std::variant<FrameError, std::vector<BufferContentRange>> ResolveInitializedBufferContents(const FrameGraphBuffer &buffer) noexcept
    {
        if (!buffer.size || !IsValidFrameBufferState(buffer.initial) || !IsValidFrameBufferState(buffer.final) ||
            buffer.initializedRanges.size() > MAX_BUFFER_INITIAL_RANGES || (buffer.initialized && !buffer.initializedRanges.empty()) ||
            (buffer.transient && (buffer.initialized || !buffer.initializedRanges.empty())))
            return Invalid("Invalid buffer initialization declaration");
        if (buffer.initialized)
            return std::vector<BufferContentRange>{{0, buffer.size}};
        auto ranges = buffer.initializedRanges;
        for (const auto &range : ranges)
            if (!range.size || range.offset >= buffer.size || range.size > buffer.size - range.offset)
                return Invalid("Invalid initialized buffer range");
        std::sort(ranges.begin(), ranges.end(), [](auto a, auto b) { return std::tie(a.offset, a.size) < std::tie(b.offset, b.size); });
        size_t merged = 0;
        for (const auto range : ranges) {
            if (merged && ranges[merged - 1].offset + ranges[merged - 1].size >= range.offset) {
                auto &previous = ranges[merged - 1];
                previous.size  = std::max(previous.offset + previous.size, range.offset + range.size) - previous.offset;
            } else {
                ranges[merged++] = range;
            }
        }
        ranges.resize(merged);
        return ranges;
    }

    bool ImageBoundaryState::operator==(const ImageBoundaryState &other) const noexcept
    {
        if (layouts.index() == other.layouts.index())
            return layouts == other.layouts;
        const auto *uniform  = std::get_if<ImageLayout>(&layouts);
        const auto &values   = std::get<std::vector<ImageLayout>>(uniform ? other.layouts : layouts);
        const auto  expected = uniform ? *uniform : std::get<ImageLayout>(other.layouts);
        return !values.empty() && std::all_of(values.begin(), values.end(), [&](auto value) { return value == expected; });
    }
    std::optional<FrameError> ValidateImageBoundaryState(const ImageBoundaryState &state, uint64_t count, bool final) noexcept
    {
        if (!count || count > MAX_FRAME_IMAGE_SUBRESOURCES)
            return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Image boundary subresource budget exceeded"};
        const auto valid = [&](ImageLayout layout) { return layout <= ImageLayout::FEEDBACK_LOOP && (!final || layout != ImageLayout::UNDEFINED); };
        if (const auto *uniform = std::get_if<ImageLayout>(&state.layouts)) {
            if (!valid(*uniform))
                return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Invalid image boundary layout"};
        } else {
            const auto &values = std::get<std::vector<ImageLayout>>(state.layouts);
            if (values.size() != count || !std::all_of(values.begin(), values.end(), valid))
                return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Image boundary table must define every mip and layer with a valid layout"};
        }
        return {};
    }

    std::variant<FrameError, ResolvedImageContents> ResolveInitializedImageContents(const FrameGraphImage &image, uint32_t rangeBudget) noexcept
    {
        rangeBudget          = std::min(rangeBudget, MAX_FRAME_IMAGE_INITIAL_RANGES);
        const auto    &desc  = image.descriptor;
        const uint64_t count = uint64_t(desc.mipLevels) * desc.arrayLayers;
        if (!count || count > MAX_FRAME_IMAGE_SUBRESOURCES || image.initializedRanges.size() + image.initializedRegions.size() > 256)
            return Invalid("Image subresource or initialization declaration budget exceeded");
        if (!desc.extent.width || !desc.extent.height || uint64_t(desc.extent.width) * desc.extent.height > UINT64_MAX / 4 ||
            desc.mipLevels > std::bit_width(std::max(desc.extent.width, desc.extent.height)))
            return Invalid("Invalid image content dimensions");
        if (auto error = ValidateImageBoundaryState(image.initial, count))
            return *error;
        if (auto error = ValidateImageBoundaryState(image.final, count, true))
            return *error;
        const bool partial = !image.initializedRanges.empty() || !image.initializedRegions.empty();
        if ((partial && image.initialized) || ((image.initialized || partial) && (image.transient || image.initial == ImageLayout::UNDEFINED)))
            return Invalid("Invalid image initialization declaration");
        ResolvedImageContents result;
        auto                 &offsets = result.subresourceOffsets;
        offsets.reserve(static_cast<size_t>(count) + 1);
        offsets.push_back(0);
        for (uint32_t layer = 0; layer < desc.arrayLayers; ++layer)
            for (uint32_t mip = 0; mip < desc.mipLevels; ++mip) {
                const uint64_t texels = uint64_t(std::max(1u, desc.extent.width >> mip)) * std::max(1u, desc.extent.height >> mip);
                if (texels > UINT64_MAX - offsets.back())
                    return Invalid("Image content address overflow");
                offsets.push_back(offsets.back() + texels);
            }
        auto reserveRanges = [&](uint32_t ranges) {
            if (ranges > rangeBudget - result.expandedRanges)
                return false;
            result.expandedRanges += ranges;
            return true;
        };
        if (image.initialized) {
            if (!reserveRanges(1))
                return Invalid("Initial image content range budget exceeded");
            result.ranges.push_back({0, offsets.back()});
        }
        const AspectFlags aspect = desc.format == PixelFormat::D32 ? AspectFlagBit::DEPTH_BIT : AspectFlagBit::COLOR_BIT;
        for (const auto &range : image.initializedRanges) {
            if (range.aspectMask != aspect || !range.levels || !range.layers || range.baseLevel >= desc.mipLevels ||
                range.levels > desc.mipLevels - range.baseLevel || range.baseLayer >= desc.arrayLayers ||
                range.layers > desc.arrayLayers - range.baseLayer)
                return Invalid("Invalid initialized image range");
            if (!reserveRanges(range.layers))
                return Invalid("Initial image content range budget exceeded");
            for (uint32_t layer = range.baseLayer; layer < range.baseLayer + range.layers; ++layer) {
                const auto first = layer * desc.mipLevels + range.baseLevel;
                result.ranges.push_back({offsets[first], offsets[first + range.levels]});
            }
        }
        if (!image.initializedRegions.empty() &&
            (desc.imageType != ImageType::IMAGE_2D ||
             (desc.format != PixelFormat::RGBA8_UNORM && desc.format != PixelFormat::RGBA8_SRGB && desc.format != PixelFormat::RGBA16_SFLOAT) ||
             desc.extent.depth != 1 || desc.samples != SampleCount::X1))
            return FrameError{FrameErrorCode::UNSUPPORTED, "Partial image contents require single-sample RGBA8/RGBA16_FLOAT 2D images"};
        for (const auto &region : image.initializedRegions) {
            if (region.mip >= desc.mipLevels || region.layer >= desc.arrayLayers)
                return Invalid("Invalid initialized image region subresource");
            const auto  width = std::max(1u, desc.extent.width >> region.mip), height = std::max(1u, desc.extent.height >> region.mip);
            const auto &rect = region.rectangle;
            if (rect.offset.x < 0 || rect.offset.y < 0 || !rect.extent.width || !rect.extent.height || uint32_t(rect.offset.x) >= width ||
                uint32_t(rect.offset.y) >= height || rect.extent.width > width - uint32_t(rect.offset.x) ||
                rect.extent.height > height - uint32_t(rect.offset.y))
                return Invalid("Invalid initialized image rectangle");
            const bool fullWidth = rect.extent.width == width;
            if (!reserveRanges(fullWidth ? 1u : rect.extent.height))
                return Invalid("Initial image content range budget exceeded");
            const auto first = offsets[region.layer * desc.mipLevels + region.mip] + uint64_t(rect.offset.y) * width + rect.offset.x;
            if (fullWidth)
                result.ranges.push_back({first, first + uint64_t(width) * rect.extent.height});
            else
                for (uint32_t row = 0; row < rect.extent.height; ++row)
                    result.ranges.push_back({first + uint64_t(row) * width, first + uint64_t(row) * width + rect.extent.width});
        }
        auto &ranges = result.ranges;
        std::sort(ranges.begin(), ranges.end(), [](auto a, auto b) { return std::tie(a.begin, a.end) < std::tie(b.begin, b.end); });
        size_t merged = 0;
        for (auto range : ranges) {
            if (merged && ranges[merged - 1].end >= range.begin)
                ranges[merged - 1].end = std::max(ranges[merged - 1].end, range.end);
            else
                ranges[merged++] = range;
        }
        ranges.resize(merged);
        auto span = ranges.begin();
        for (uint32_t subresource = 0; span != ranges.end() && subresource < count; ++subresource) {
            while (span != ranges.end() && span->end <= offsets[subresource])
                ++span;
            if (span != ranges.end() && span->begin < offsets[subresource + 1] && image.initial.At(subresource) == ImageLayout::UNDEFINED)
                return Invalid("Initialized texels require a defined initial layout on their subresource");
        }
        return result;
    }

    std::variant<FrameError, RasterBufferRange> ResolveFrameBufferRange(const FrameBufferBinding &binding, uint64_t bufferSize) noexcept
    {
        auto range = binding.range;
        if (binding.access > FrameAccessMode::READ_WRITE || binding.kind > FrameBufferBindingKind::STORAGE || !range.size ||
            range.offset >= bufferSize || binding.dynamicOffset > bufferSize - range.offset)
            return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Invalid declared buffer range/access"};
        range.offset += binding.dynamicOffset;
        if (range.size > bufferSize - range.offset)
            return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Declared buffer range exceeds storage"};
        if (binding.kind == FrameBufferBindingKind::STORAGE) {
            const auto stride = binding.bufferKind == ShaderBufferKind::RAW ? 4u : binding.elementStride;
            if ((binding.resourceAccess != ShaderResourceAccess::READ_ONLY && binding.resourceAccess != ShaderResourceAccess::READ_WRITE) ||
                (binding.resourceAccess == ShaderResourceAccess::READ_ONLY && binding.access != FrameAccessMode::READ) ||
                (binding.bufferKind != ShaderBufferKind::RAW && binding.bufferKind != ShaderBufferKind::STRUCTURED) ||
                (binding.bufferKind == ShaderBufferKind::RAW && binding.elementStride) || !stride || stride % 4 || range.offset % stride ||
                range.size % stride)
                return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Invalid explicit storage buffer view or read-only access"};
        }
        return range;
    }

    std::variant<FrameError, RasterImageView>
    ResolveFrameImageView(const FrameImageBinding &binding, const Image::Descriptor &desc, bool storage) noexcept
    {
        if (binding.access > FrameAccessMode::READ_WRITE || binding.kind > FrameImageBindingKind::STORAGE ||
            (binding.view.type != ImageViewType::VIEW_2D && binding.view.type != ImageViewType::VIEW_2D_ARRAY))
            return Invalid("Invalid declared image view range/access");
        const auto resolved = NormalizeImageView(binding.view, desc, "Invalid declared image view range/access");
        if (const auto *error = std::get_if<FrameError>(&resolved))
            return *error;
        const auto &view = std::get<RasterImageView>(resolved);
        if (storage && (view.mipCount != 1 || desc.samples != SampleCount::X1))
            return Invalid("Compute storage images require a single mip and sample");
        return view;
    }

    std::vector<uint32_t> ResolveComputeDynamicOffsets(const FrameComputePass &pass, uint32_t set)
    {
        const FrameBindingLookup<FrameBufferBinding> buffers(pass.buffers);
        std::vector<std::pair<uint32_t, uint32_t>>   ordered;
        for (const auto &resource : pass.shader->GetReflection().resources) {
            if (resource.set != set ||
                (resource.type != ShaderResourceType::UNIFORM_BUFFER_DYNAMIC && resource.type != ShaderResourceType::STORAGE_BUFFER_DYNAMIC))
                continue;
            const auto *found = buffers.Find(set, resource.binding);
            if (found != nullptr)
                ordered.emplace_back(resource.binding, found->dynamicOffset);
        }
        std::sort(ordered.begin(), ordered.end());
        std::vector<uint32_t> offsets;
        offsets.reserve(ordered.size());
        for (const auto &[binding, offset] : ordered)
            offsets.push_back(offset);
        return offsets;
    }

    namespace {
        // Called after handles, views and reflected bindings have been validated.
        template <class Pass>
        std::optional<FrameError>
        ValidatePassKinds(const FrameGraphData &data, const Pass &pass, const FrameBindingLookup<ShaderResource> *reflection)
        {
            std::vector<FrameBufferBindingKind> bufferKinds(data.buffers.size(), FrameBufferBindingKind::REFLECTED);
            for (const auto &binding : pass.buffers) {
                auto kind = binding.kind;
                if (reflection) {
                    const auto *resource = reflection->Find(binding.set, binding.binding);
                    if (!resource)
                        return Invalid("Missing compute buffer reflection");
                    kind = resource->type == ShaderResourceType::UNIFORM_BUFFER || resource->type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC
                               ? FrameBufferBindingKind::UNIFORM
                               : FrameBufferBindingKind::STORAGE;
                }
                const auto index    = binding.range.buffer.index;
                auto      &previous = bufferKinds[index];
                if (previous == kind)
                    continue;
                if (previous != FrameBufferBindingKind::REFLECTED)
                    return Invalid("A pass cannot use one buffer as both uniform and storage");
                const auto *physical = data.buffers[index].buffer.Get();
                if (physical)
                    for (size_t j = 0; j < data.buffers.size(); ++j)
                        if (data.buffers[j].buffer.Get() == physical && bufferKinds[j] != FrameBufferBindingKind::REFLECTED && bufferKinds[j] != kind)
                            return Invalid("A pass cannot use one physical buffer as both uniform and storage");
                previous = kind;
            }
            std::vector<std::vector<FrameImageBindingKind>> imageKinds(data.images.size());
            for (const auto &binding : pass.images) {
                auto kind = binding.kind;
                if (reflection) {
                    const auto *resource = reflection->Find(binding.set, binding.binding);
                    if (!resource)
                        return Invalid("Missing compute image reflection");
                    kind = resource->type == ShaderResourceType::STORAGE_IMAGE ? FrameImageBindingKind::STORAGE : FrameImageBindingKind::SAMPLED;
                }
                const auto &desc     = data.images[binding.image.index].descriptor;
                const auto  resolved = ResolveFrameImageView(binding, desc);
                if (const auto *error = std::get_if<FrameError>(&resolved))
                    return *error;
                const auto &view  = std::get<RasterImageView>(resolved);
                auto       &cells = imageKinds[binding.image.index];
                if (cells.empty())
                    cells.resize(uint64_t(desc.mipLevels) * desc.arrayLayers, FrameImageBindingKind::REFLECTED);
                for (uint32_t layer = view.baseLayer; layer < view.baseLayer + view.layerCount; ++layer)
                    for (uint32_t mip = view.baseMip; mip < view.baseMip + view.mipCount; ++mip) {
                        auto &previous = cells[uint64_t(layer) * desc.mipLevels + mip];
                        if (previous != FrameImageBindingKind::REFLECTED && previous != kind)
                            return Invalid("A pass cannot use overlapping image views as both sampled and storage");
                        previous = kind;
                    }
            }
            return {};
        }
        std::optional<FrameError> ValidateComputeBindings(const FrameComputePass &pass, const ShaderReflection &reflection)
        {
            if (!pass.shader)
                return Invalid("Compute pass requires a shader");
            if (!pass.groupsX || !pass.groupsY || !pass.groupsZ)
                return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Compute dispatch dimensions must be nonzero"};
            if (!reflection.pushConstants.empty())
                return FrameError{FrameErrorCode::UNSUPPORTED, "Compute push constants are not implemented"};
            if (pass.buffers.size() + pass.images.size() + pass.samplers.size() != reflection.resources.size())
                return Invalid("Compute bindings do not match shader reflection");
            const FrameBindingLookup<FrameBufferBinding>   buffers(pass.buffers);
            const FrameBindingLookup<FrameImageBinding>    images(pass.images);
            const FrameBindingLookup<RasterSamplerBinding> samplers(pass.samplers);
            for (size_t i = 0; i < reflection.resources.size(); ++i) {
                const auto &resource = reflection.resources[i];
                if (resource.count != 1 || resource.descriptorArray || resource.set > 2 || resource.binding >= 32)
                    return FrameError{FrameErrorCode::UNSUPPORTED, "Unsupported compute descriptor shape"};
                for (size_t j = 0; j < i; ++j)
                    if (reflection.resources[j].set == resource.set && reflection.resources[j].binding == resource.binding)
                        return Invalid("Duplicate reflected compute binding");
                switch (resource.type) {
                case ShaderResourceType::SAMPLER: {
                    const auto *found = samplers.Find(resource.set, resource.binding);
                    if (!found || !found->sampler || resource.sampler != ShaderSamplerType::REGULAR)
                        return Invalid("Missing compute sampler binding");
                    break;
                }
                case ShaderResourceType::SAMPLED_IMAGE: {
                    const auto *found = images.Find(resource.set, resource.binding);
                    if (!found || found->access != FrameAccessMode::READ ||
                        (found->kind != FrameImageBindingKind::REFLECTED && found->kind != FrameImageBindingKind::SAMPLED) ||
                        resource.texture.dimension != ShaderTextureDimension::D2 || resource.texture.multisampled ||
                        resource.texture.arrayed != (found->view.type == ImageViewType::VIEW_2D_ARRAY))
                        return Invalid("Invalid compute sampled image binding");
                    break;
                }
                case ShaderResourceType::STORAGE_IMAGE: {
                    const auto *found = images.Find(resource.set, resource.binding);
                    if (!found || (resource.access == ShaderResourceAccess::READ_ONLY && found->access != FrameAccessMode::READ) ||
                        resource.texture.dimension != ShaderTextureDimension::D2 || resource.texture.multisampled ||
                        (found->kind != FrameImageBindingKind::REFLECTED && found->kind != FrameImageBindingKind::STORAGE) ||
                        resource.texture.arrayed != (found->view.type == ImageViewType::VIEW_2D_ARRAY))
                        return Invalid("Invalid compute storage image binding");
                    break;
                }
                case ShaderResourceType::UNIFORM_BUFFER:
                case ShaderResourceType::UNIFORM_BUFFER_DYNAMIC: {
                    const auto *found = buffers.Find(resource.set, resource.binding);
                    if (!found || found->access != FrameAccessMode::READ ||
                        (found->kind != FrameBufferBindingKind::REFLECTED && found->kind != FrameBufferBindingKind::UNIFORM) ||
                        (resource.type == ShaderResourceType::UNIFORM_BUFFER && found->dynamicOffset))
                        return Invalid("Invalid compute uniform buffer binding");
                    break;
                }
                case ShaderResourceType::STORAGE_BUFFER:
                case ShaderResourceType::STORAGE_BUFFER_DYNAMIC: {
                    const auto *found = buffers.Find(resource.set, resource.binding);
                    if (!found || (found->kind != FrameBufferBindingKind::REFLECTED && found->kind != FrameBufferBindingKind::STORAGE) ||
                        (resource.type == ShaderResourceType::STORAGE_BUFFER && found->dynamicOffset) ||
                        (resource.access == ShaderResourceAccess::READ_ONLY && found->access != FrameAccessMode::READ) ||
                        (resource.access != ShaderResourceAccess::READ_ONLY && resource.access != ShaderResourceAccess::READ_WRITE))
                        return Invalid("Invalid compute storage buffer binding");
                    const auto stride = resource.bufferKind == ShaderBufferKind::RAW ? 4u : resource.elementStride;
                    if ((resource.bufferKind != ShaderBufferKind::RAW && resource.bufferKind != ShaderBufferKind::STRUCTURED) ||
                        (resource.bufferKind == ShaderBufferKind::RAW && resource.elementStride) || !stride || stride % 4 ||
                        found->range.offset % stride || found->dynamicOffset % stride || found->range.size % stride)
                        return Invalid("Compute storage buffer range does not match the shader storage shape");
                    break;
                }
                default: return FrameError{FrameErrorCode::UNSUPPORTED, "Unsupported compute resource type"};
                }
            }
            for (const auto &block : reflection.blocks) {
                const auto *found = buffers.Find(block.set, block.binding);
                if (!found || found->access != FrameAccessMode::READ || !block.size || block.size > found->range.size)
                    return Invalid("Compute uniform range does not cover reflected block");
            }
            return {};
        }
    } // namespace

    namespace {
        struct Use {
            uint32_t resource;
            Range    range;
        };
        struct Accesses {
            std::vector<Use> reads, writes, defines;
            bool             sideEffect = false;
        };
        class FrameGraphAnalyzer {
        public:
            explicit FrameGraphAnalyzer(const FrozenFrameGraph &graph)
                : graph(graph), data(graph.GetData()), imageBase(static_cast<uint32_t>(data.buffers.size()))
            {
            }

            FrameGraphAnalysisResult Analyze()
            {
                if (auto error = ValidateResources())
                    return *error;
                if (auto error = CollectPassAccesses())
                    return *error;
                if (auto error = CollectExports())
                    return *error;
                return ValidateContents(CullPasses());
            }

        private:
            bool ValidBuffer(GraphBufferHandle handle, uint64_t offset, uint64_t size) const
            {
                return handle.graph == graph.GetId() && handle.index < data.buffers.size() && size && offset < data.buffers[handle.index].size &&
                       size <= data.buffers[handle.index].size - offset;
            }

            bool ValidImage(GraphImageHandle handle) const
            {
                return handle.graph == graph.GetId() && handle.index < data.images.size();
            }

            std::optional<FrameError> ValidateResources()
            {
                if (!graph.GetId() || data.buffers.size() > 256 || data.images.size() > 256 || data.passes.size() > 1024 ||
                    data.exports.size() > 256 || data.imageExports.size() > 256)
                    return Invalid("Invalid graph identity or graph budget exceeded");
                initialBuffers.resize(data.buffers.size());
                for (size_t i = 0; i < data.buffers.size(); ++i) {
                    const auto &buffer = data.buffers[i];
                    if (bool(buffer.buffer) == bool(buffer.transient) || !buffer.size || !IsValidFrameBufferState(buffer.initial) ||
                        !IsValidFrameBufferState(buffer.final))
                        return Invalid("Invalid buffer import");
                    if (!buffer.initializedRanges.empty()) {
                        const auto contents = ResolveInitializedBufferContents(buffer);
                        if (const auto *error = std::get_if<FrameError>(&contents))
                            return *error;
                        for (const auto &range : std::get<std::vector<BufferContentRange>>(contents))
                            initialBuffers[i].push_back({range.offset, range.offset + range.size});
                    }
                    if (buffer.transient) {
                        if (buffer.initialized || buffer.initial != CopyBufferState::COMMON || buffer.final != CopyBufferState::COMMON ||
                            buffer.size != buffer.transient->size || !buffer.transient->usage)
                            return Invalid("Invalid transient buffer declaration");
                        if (buffer.transient->memory != MemoryType::GPU_ONLY)
                            return FrameError{FrameErrorCode::UNSUPPORTED};
                    }
                    for (size_t j = 0; j < i; ++j)
                        if (buffer.buffer && data.buffers[j].buffer.Get() == buffer.buffer.Get())
                            return Invalid("Duplicate physical buffer import");
                }
                initialContents.resize(data.images.size());
                uint32_t initialRangeBudget = MAX_FRAME_IMAGE_INITIAL_RANGES;
                imageOffsets.resize(data.images.size());
                uint64_t imageSubresources = 0;
                for (size_t i = 0; i < data.images.size(); ++i) {
                    const auto &image = data.images[i];
                    const auto &desc  = image.descriptor;
                    if (bool(image.image) == image.transient || !desc.extent.width || !desc.extent.height ||
                        uint64_t(desc.extent.width) * desc.extent.height > UINT64_MAX / 4 || !desc.mipLevels || !desc.arrayLayers ||
                        desc.mipLevels > std::bit_width(std::max(desc.extent.width, desc.extent.height)) ||
                        uint64_t(desc.mipLevels) * desc.arrayLayers > UINT32_MAX || (image.initial == ImageLayout::UNDEFINED && image.initialized))
                        return Invalid("Invalid image import");
                    if (!image.image.HasOwner() ||
                        (image.image.IsBorrowed() &&
                         (image.transient || image.initialized || desc.samples != SampleCount::X1 || desc.mipLevels != 1 || desc.arrayLayers != 1 ||
                          image.initial != ImageLayout::UNDEFINED || image.final != ImageLayout::PRESENT)))
                        return Invalid("Presentation images require a borrowed output lease");
                    if (!image.image.IsBorrowed())
                        for (const auto *boundary : {&image.initial, &image.final}) {
                            const auto *uniform = std::get_if<ImageLayout>(&boundary->layouts);
                            if (uniform ? *uniform == ImageLayout::PRESENT
                                        : std::ranges::find(std::get<std::vector<ImageLayout>>(boundary->layouts), ImageLayout::PRESENT) !=
                                              std::get<std::vector<ImageLayout>>(boundary->layouts).end())
                                return FrameError{FrameErrorCode::UNSUPPORTED, "PRESENT requires a borrowed output lease"};
                        }
                    imageSubresources += uint64_t(desc.mipLevels) * desc.arrayLayers;
                    if (imageSubresources > MAX_FRAME_IMAGE_SUBRESOURCES)
                        return Invalid("Frame image subresource budget exceeded");
                    auto contents = ResolveInitializedImageContents(image, initialRangeBudget);
                    if (const auto *error = std::get_if<FrameError>(&contents))
                        return *error;
                    auto &resolved = std::get<ResolvedImageContents>(contents);
                    initialRangeBudget -= resolved.expandedRanges;
                    initialContents[i] = std::move(resolved.ranges);
                    imageOffsets[i]    = std::move(resolved.subresourceOffsets);
                    if (image.transient) {
                        if (image.initialized || image.initial != ImageLayout::UNDEFINED || image.final != ImageLayout::GENERAL || !desc.usage)
                            return Invalid("Invalid transient image declaration");
                        if (desc.memory != MemoryType::GPU_ONLY || desc.usage.TestBit(ImageUsageFlagBit::TRANSIENT) || desc.arrayLayers > UINT16_MAX)
                            return FrameError{FrameErrorCode::UNSUPPORTED};
                        if (desc.viewUsage.TestBit(ImageViewUsageFlagBit::CUBE_MAP_COMPATIBLE) &&
                            (desc.extent.width != desc.extent.height || desc.arrayLayers < 6 || desc.arrayLayers % 6))
                            return Invalid("Invalid transient cube image");
                        if ((desc.format == PixelFormat::D32 &&
                             (desc.usage.TestBit(ImageUsageFlagBit::RENDER_TARGET) || desc.usage.TestBit(ImageUsageFlagBit::STORAGE))) ||
                            (ColorFormat(desc.format) && desc.usage.TestBit(ImageUsageFlagBit::DEPTH_STENCIL)))
                            return Invalid("Transient image format/usage mismatch");
                    }
                    if (desc.imageType != ImageType::IMAGE_2D ||
                        (image.image.IsBorrowed() ? !ColorFormat(desc.format)
                                                  : (desc.format != PixelFormat::RGBA8_UNORM && desc.format != PixelFormat::RGBA8_SRGB &&
                                                     desc.format != PixelFormat::RGBA16_SFLOAT && desc.format != PixelFormat::D32)) ||
                        desc.extent.depth != 1)
                        return FrameError{FrameErrorCode::UNSUPPORTED, "Only RGBA8/RGBA16_FLOAT/D32 2D images are supported"};
                    if (desc.samples != SampleCount::X1 &&
                        ((desc.samples != SampleCount::X2 && desc.samples != SampleCount::X4 && desc.samples != SampleCount::X8) ||
                         desc.mipLevels != 1 || desc.memory != MemoryType::GPU_ONLY || desc.viewUsage ||
                         desc.usage !=
                             ImageUsageFlags(desc.format == PixelFormat::D32 ? ImageUsageFlagBit::DEPTH_STENCIL : ImageUsageFlagBit::RENDER_TARGET)))
                        return FrameError{FrameErrorCode::UNSUPPORTED, "MSAA requires 2/4/8-sample RGBA8 color or D32 depth storage with one mip"};
                    for (size_t j = 0; j < i; ++j)
                        if (image.image && data.images[j].image.Get() == image.image.Get())
                            return Invalid("Duplicate physical image import");
                }

                return {};
            }

            std::optional<FrameError>
            AddDeclaredBuffer(Accesses &out, const FrameBufferBinding &binding, bool descriptor, FrameBufferBindingKind kind)
            {
                if ((descriptor && (binding.set > 2 || binding.binding >= 32)) || binding.range.buffer.graph != graph.GetId() ||
                    binding.range.buffer.index >= data.buffers.size())
                    return Invalid("Invalid declared buffer binding");
                const auto &input    = data.buffers[binding.range.buffer.index];
                const auto  resolved = ResolveFrameBufferRange(binding, input.size);
                if (const auto *error = std::get_if<FrameError>(&resolved))
                    return *error;
                const auto &range = std::get<RasterBufferRange>(resolved);
                const bool  read  = binding.access != FrameAccessMode::WRITE;
                const bool  write = binding.access != FrameAccessMode::READ;
                if (input.transient) {
                    const auto usage = input.transient->usage;
                    if (!usage.TestBit(kind == FrameBufferBindingKind::UNIFORM ? BufferUsageFlagBit::UNIFORM : BufferUsageFlagBit::STORAGE))
                        return Invalid("Declared buffer kind does not match usage");
                }
                const Use use{range.buffer.index, {range.offset, range.offset + range.size}};
                if (read)
                    out.reads.push_back(use);
                if (write) {
                    out.writes.push_back(use);
                    out.defines.push_back(use);
                }
                return {};
            }

            std::optional<FrameError> AddDeclaredImage(Accesses &out, const FrameImageBinding &binding, bool descriptor, bool storage)
            {
                if ((descriptor && (binding.set > 2 || binding.binding >= 32)) || !ValidImage(binding.image))
                    return Invalid("Invalid declared image binding");
                const auto &input = data.images[binding.image.index];
                const auto &desc  = input.descriptor;
                // Custom may span mips; the graph-wide MSAA usage check already excludes storage images.
                const auto resolved = ResolveFrameImageView(binding, desc, storage && descriptor);
                if (const auto *error = std::get_if<FrameError>(&resolved))
                    return *error;
                const auto &view  = std::get<RasterImageView>(resolved);
                const bool  read  = binding.access != FrameAccessMode::WRITE;
                const bool  write = binding.access != FrameAccessMode::READ;
                if (read && !storage && !desc.usage.TestBit(ImageUsageFlagBit::SAMPLED))
                    return Invalid("Declared image read lacks sampled usage");
                if ((write || storage) && !desc.usage.TestBit(ImageUsageFlagBit::STORAGE))
                    return Invalid("Declared image write lacks storage usage");
                for (uint32_t layer = view.baseLayer; layer < view.baseLayer + view.layerCount; ++layer) {
                    const auto  first   = uint64_t(layer) * desc.mipLevels + view.baseMip;
                    const auto &offsets = imageOffsets[binding.image.index];
                    const Use   use{static_cast<uint32_t>(imageBase) + binding.image.index, {offsets[first], offsets[first + view.mipCount]}};
                    if (read)
                        out.reads.push_back(use);
                    if (write) {
                        out.writes.push_back(use);
                        out.defines.push_back(use);
                    }
                }
                return {};
            }

            std::optional<FrameError> CollectDraw(const RasterDraw &draw, const FrameRasterPass &raster, Extent2D extent, Accesses &access)
            {
                auto readBuffer = [&](const RasterBufferRange &range, uint64_t extra = 0) {
                    if (extra > UINT64_MAX - range.offset || !ValidBuffer(range.buffer, range.offset + extra, range.size))
                        return false;
                    access.reads.push_back({range.buffer.index, {range.offset + extra, range.offset + extra + range.size}});
                    return true;
                };
                const auto &r = draw.scissor;
                if (!draw.shader || (!draw.indexed && (!draw.command.vertexCount || !draw.command.instanceCount)))
                    return Invalid("Invalid raster draw");
                if (draw.vertices.size() > MAX_VERTEX_BINDINGS || draw.attributes.size() > 16 ||
                    draw.uniforms.size() + draw.images.size() + draw.samplers.size() > 32 || draw.depth.compare > CompareOp::ALWAYS ||
                    (draw.depth.write && !draw.depth.test) || ((draw.depth.test || draw.depth.write) && !raster.depth))
                    return Invalid("Invalid raster input budget or depth state");
                if (draw.depth.test && raster.depth->load == LoadOp::DONT_CARE)
                    return Invalid("Depth test reads undefined depth");
                if (draw.indexed) {
                    const auto &indexed = *draw.indexed;
                    const auto  bytes   = indexed.type == IndexType::U16 ? 2u : 4u;
                    if ((indexed.type != IndexType::U16 && indexed.type != IndexType::U32) || !indexed.command.indexCount ||
                        !indexed.command.instanceCount || indexed.range.offset % bytes ||
                        (uint64_t(indexed.command.firstIndex) + indexed.command.indexCount) * bytes > indexed.range.size ||
                        indexed.range.size > UINT32_MAX || !readBuffer(indexed.range))
                        return Invalid("Invalid index input range");
                }
                for (const auto &stream : draw.vertices) {
                    if (!stream.stride || stream.stride > 2048 || stream.range.size > UINT32_MAX || stream.range.offset % 4 || stream.stride % 4 ||
                        stream.rate > VertexInputRate::PER_INSTANCE || stream.range.size < stream.stride || !readBuffer(stream.range))
                        return Invalid("Invalid vertex input range/stride");
                    const auto instanceCount = draw.indexed ? draw.indexed->command.instanceCount : draw.command.instanceCount;
                    const auto firstInstance = draw.indexed ? draw.indexed->command.firstInstance : draw.command.firstInstance;
                    if ((stream.rate == VertexInputRate::PER_INSTANCE &&
                         (uint64_t(firstInstance) + instanceCount) * stream.stride > stream.range.size) ||
                        (!draw.indexed && stream.rate == VertexInputRate::PER_VERTEX &&
                         (uint64_t(draw.command.firstVertex) + draw.command.vertexCount) * stream.stride > stream.range.size))
                        return Invalid("Draw exceeds vertex/instance range");
                }
                for (size_t a = 0; a < draw.attributes.size(); ++a) {
                    const auto &attr = draw.attributes[a];
                    if (attr.format < Format::F_R32 || attr.format > Format::F_RGBA32)
                        return FrameError{FrameErrorCode::UNSUPPORTED, "Raster supports float vertex attributes"};
                    const auto bytes = static_cast<uint32_t>(attr.format) * 4;
                    if (attr.binding >= draw.vertices.size() || attr.location >= 16 || attr.offset % 4 || attr.semantic.empty() ||
                        attr.semantic.size() > 64 || attr.semantic.find('\0') != std::string::npos ||
                        attr.offset > draw.vertices[attr.binding].stride || bytes > draw.vertices[attr.binding].stride - attr.offset)
                        return Invalid("Invalid vertex attribute");
                    for (size_t b = 0; b < a; ++b)
                        if (draw.attributes[b].location == attr.location ||
                            (draw.attributes[b].semantic == attr.semantic && draw.attributes[b].semanticIndex == attr.semanticIndex))
                            return Invalid("Duplicate vertex attribute");
                }
                bool occupied[3][32]{};
                auto bind = [&](uint32_t set, uint32_t binding) {
                    if (set > 2 || binding >= 32 || occupied[set][binding])
                        return false;
                    occupied[set][binding] = true;
                    return true;
                };
                for (const auto &binding : draw.uniforms) {
                    if (!bind(binding.set, binding.binding) || binding.range.size > 65536 || !readBuffer(binding.range, binding.dynamicOffset))
                        return Invalid("Invalid uniform input range/set");
                }
                const FrameBindingLookup<ShaderResource> reflection(draw.shader->GetReflection().resources);
                for (const auto &binding : draw.images) {
                    if (!bind(binding.set, binding.binding) || !ValidImage(binding.image) || binding.image == raster.color ||
                        (raster.depth && binding.image == raster.depth->image) ||
                        (raster.colorResolve && binding.image == raster.colorResolve->image) ||
                        (raster.depth && raster.depth->resolve && binding.image == raster.depth->resolve->image))
                        return Invalid("Invalid sampled image binding or attachment feedback");
                    const auto &sampled  = data.images[binding.image.index].descriptor;
                    const auto *resource = reflection.Find(binding.set, binding.binding);
                    if (sampled.format == PixelFormat::D32 &&
                        (!resource || resource->type != ShaderResourceType::SAMPLED_IMAGE ||
                         resource->texture.scalarType != ShaderScalarType::FLOAT || resource->texture.components != 1))
                        return Invalid("Depth sampling requires a float scalar texture binding");
                    if (resource && sampled.format != PixelFormat::D32 && resource->texture.components != 4)
                        return Invalid("Color sampling requires a float4 texture binding");
                    auto resolved = ResolveRasterImageView(binding.view, sampled);
                    if (const auto *error = std::get_if<FrameError>(&resolved))
                        return *error;
                    const auto &view = std::get<RasterImageView>(resolved);
                    for (uint32_t layer = view.baseLayer; layer < view.baseLayer + view.layerCount; ++layer) {
                        const auto  first   = uint64_t(layer) * sampled.mipLevels + view.baseMip;
                        const auto &offsets = imageOffsets[binding.image.index];
                        access.reads.push_back({imageBase + binding.image.index, {offsets[first], offsets[first + view.mipCount]}});
                    }
                }
                for (const auto &binding : draw.samplers)
                    if (!bind(binding.set, binding.binding) || !binding.sampler)
                        return Invalid("Invalid sampler binding");
                const auto &blend = draw.blend;
                if (blend.writeMask > 0xF || blend.srcColor > BlendFactor::ONE_MINUS_SRC1_ALPHA ||
                    blend.dstColor > BlendFactor::ONE_MINUS_SRC1_ALPHA || blend.srcAlpha > BlendFactor::ONE_MINUS_SRC1_ALPHA ||
                    blend.dstAlpha > BlendFactor::ONE_MINUS_SRC1_ALPHA || blend.colorBlendOp > BlendOp::SUBTRACT ||
                    blend.alphaBlendOp > BlendOp::SUBTRACT)
                    return Invalid("Invalid raster blend state");
                if (r.offset.x < 0 || r.offset.y < 0 || uint32_t(r.offset.x) >= extent.width || uint32_t(r.offset.y) >= extent.height ||
                    r.extent.width > extent.width - r.offset.x || r.extent.height > extent.height - r.offset.y ||
                    ((!r.extent.width || !r.extent.height) && (r.extent.width || r.extent.height || r.offset.x || r.offset.y)))
                    return Invalid("Invalid draw scissor");

                return {};
            }

            std::optional<FrameError> CollectAccess(const BufferCopyPass &copy, Accesses &access)
            {
                if (!ValidBuffer(copy.source, copy.sourceOffset, copy.size) || !ValidBuffer(copy.destination, copy.destinationOffset, copy.size) ||
                    copy.source == copy.destination)
                    return Invalid("Invalid buffer copy range or same-buffer copy");
                access.reads.push_back({copy.source.index, {copy.sourceOffset, copy.sourceOffset + copy.size}});
                access.writes.push_back({copy.destination.index, {copy.destinationOffset, copy.destinationOffset + copy.size}});
                access.defines = access.writes;

                return {};
            }
            std::optional<FrameError> CollectAccess(const BufferImageCopyPass &copy, Accesses &access)
            {
                if (!ValidImage(copy.image) || copy.direction > ImageCopyDirection::IMAGE_TO_BUFFER)
                    return Invalid("Invalid image copy handle/direction");
                const auto &desc        = data.images[copy.image.index].descriptor;
                const auto &region      = copy.region;
                const auto &subresource = region.subRange;
                if (subresource.level >= desc.mipLevels || subresource.baseLayer >= desc.arrayLayers || !subresource.layers ||
                    subresource.layers > desc.arrayLayers - subresource.baseLayer)
                    return Invalid("Invalid image copy mip/layer");
                const auto width  = std::max(1u, desc.extent.width >> subresource.level);
                const auto height = std::max(1u, desc.extent.height >> subresource.level);
                if (!region.imageExtent.width || !region.imageExtent.height || region.imageExtent.depth != 1 || region.imageOffset.x < 0 ||
                    region.imageOffset.y < 0 || region.imageOffset.z || uint32_t(region.imageOffset.x) >= width ||
                    uint32_t(region.imageOffset.y) >= height || region.imageExtent.width > width - region.imageOffset.x ||
                    region.imageExtent.height > height - region.imageOffset.y)
                    return Invalid("Invalid image copy extent/offset");
                if (desc.samples != SampleCount::X1 ||
                    (desc.format != PixelFormat::RGBA8_UNORM && desc.format != PixelFormat::RGBA8_SRGB &&
                     desc.format != PixelFormat::RGBA16_SFLOAT) ||
                    subresource.layers != 1 || (subresource.aspectMask && subresource.aspectMask != AspectFlagBit::COLOR_BIT))
                    return FrameError{FrameErrorCode::UNSUPPORTED, "Image copies require one RGBA8/RGBA16_FLOAT color mip/layer"};
                if ((region.bufferRowLength && region.bufferRowLength < region.imageExtent.width) ||
                    (region.bufferImageHeight && region.bufferImageHeight < region.imageExtent.height))
                    return Invalid("Image copy buffer pitch is smaller than the rectangle");
                const uint32_t texelBytes = desc.format == PixelFormat::RGBA16_SFLOAT ? 8 : 4;
                const uint64_t rowBytes   = uint64_t(region.imageExtent.width) * texelBytes;
                const uint64_t rowPitch   = uint64_t(region.bufferRowLength ? region.bufferRowLength : region.imageExtent.width) * texelBytes;
                if (rowPitch > INT32_MAX)
                    return Invalid("Image copy row pitch exceeds the supported byte range");
                const uint64_t bytes = uint64_t(region.imageExtent.height - 1) * rowPitch + rowBytes;
                if (region.bufferOffset % texelBytes || !ValidBuffer(copy.buffer, region.bufferOffset, bytes))
                    return Invalid("Invalid image copy buffer range/alignment");
                const auto unit        = subresource.baseLayer * desc.mipLevels + subresource.level;
                const auto imageOffset = imageOffsets[copy.image.index][unit] + uint64_t(region.imageOffset.y) * width + region.imageOffset.x;
                const auto bufferRows  = rowPitch == rowBytes ? 1u : region.imageExtent.height;
                const auto imageRows   = region.imageExtent.width == width ? 1u : region.imageExtent.height;
                copyRanges += uint64_t(bufferRows) + imageRows;
                if (copyRanges > MAX_FRAME_IMAGE_COPY_RANGES)
                    return Invalid("Image copy content range budget exceeded");
                auto &bufferUses = copy.direction == ImageCopyDirection::BUFFER_TO_IMAGE ? access.reads : access.writes;
                auto &imageUses  = copy.direction == ImageCopyDirection::BUFFER_TO_IMAGE ? access.writes : access.reads;
                for (uint32_t row = 0; row < bufferRows; ++row) {
                    const auto first = region.bufferOffset + row * rowPitch;
                    bufferUses.push_back({copy.buffer.index, {first, first + (bufferRows == 1 ? bytes : rowBytes)}});
                }
                for (uint32_t row = 0; row < imageRows; ++row) {
                    const auto     first = imageOffset + uint64_t(row) * width;
                    const uint64_t count = imageRows == 1 ? uint64_t(region.imageExtent.width) * region.imageExtent.height : region.imageExtent.width;
                    imageUses.push_back({imageBase + copy.image.index, {first, first + count}});
                }
                access.defines = access.writes;

                return {};
            }
            std::optional<FrameError> CollectAccess(const FrameComputePass &compute, Accesses &access)
            {
                if (!compute.shader)
                    return Invalid("Compute pass requires a shader");
                if (auto error = ValidateComputeBindings(compute, compute.shader->GetReflection()))
                    return *error;
                const FrameBindingLookup<ShaderResource> resources(compute.shader->GetReflection().resources);
                for (const auto &binding : compute.buffers) {
                    const auto *resource = resources.Find(binding.set, binding.binding);
                    if (!resource)
                        return Invalid("Missing compute buffer reflection");
                    const bool uniform =
                        resource->type == ShaderResourceType::UNIFORM_BUFFER || resource->type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC;
                    if (auto error =
                            AddDeclaredBuffer(access, binding, true, uniform ? FrameBufferBindingKind::UNIFORM : FrameBufferBindingKind::STORAGE))
                        return *error;
                }
                for (const auto &binding : compute.images) {
                    const auto *resource = resources.Find(binding.set, binding.binding);
                    if (!resource)
                        return Invalid("Missing compute image reflection");
                    if (auto error = AddDeclaredImage(access, binding, true, resource->type == ShaderResourceType::STORAGE_IMAGE))
                        return *error;
                }
                if (auto error = ValidatePassKinds(data, compute, &resources))
                    return *error;

                return {};
            }
            std::optional<FrameError> CollectAccess(const FrameCustomPass &custom, Accesses &access)
            {
                if (!custom.encoder || (custom.buffers.empty() && custom.images.empty() && !custom.sideEffect))
                    return Invalid("Custom pass requires an encoder and declared work");
                for (const auto &binding : custom.buffers) {
                    if ((binding.kind != FrameBufferBindingKind::UNIFORM && binding.kind != FrameBufferBindingKind::STORAGE) ||
                        (binding.kind == FrameBufferBindingKind::UNIFORM && binding.access != FrameAccessMode::READ))
                        return Invalid("Custom buffers require explicit uniform/read or storage access");
                    if (auto error = AddDeclaredBuffer(access, binding, false, binding.kind))
                        return *error;
                }
                for (const auto &binding : custom.images) {
                    if ((binding.kind != FrameImageBindingKind::SAMPLED && binding.kind != FrameImageBindingKind::STORAGE) ||
                        (binding.kind == FrameImageBindingKind::SAMPLED && binding.access != FrameAccessMode::READ))
                        return Invalid("Custom images require explicit sampled/read or storage access");
                    if (auto error = AddDeclaredImage(access, binding, false, binding.kind == FrameImageBindingKind::STORAGE))
                        return *error;
                }
                if (auto error = ValidatePassKinds(data, custom, nullptr))
                    return *error;

                return {};
            }
            std::optional<FrameError> CollectAccess(const FrameRasterPass &raster, Accesses &access)
            {
                const bool hasColor = raster.color != GraphImageHandle{};
                if ((hasColor && !ValidImage(raster.color)) || (!hasColor && !raster.depth) || (raster.depth && !ValidImage(raster.depth->image)) ||
                    raster.load > LoadOp::CLEAR || raster.store > StoreOp::STORE || raster.draws.size() > 4096)
                    return Invalid("Invalid raster attachment or draw budget");
                if ((!hasColor && (raster.colorResolve || raster.colorLocalRead || raster.colorView != RasterAttachmentView{})) ||
                    (raster.colorLocalRead && raster.load != LoadOp::LOAD))
                    return Invalid("Color local read requires loaded contents");
                const auto  attachment    = hasColor ? raster.color : raster.depth->image;
                const auto &color         = data.images[attachment.index].descriptor;
                auto        colorResolved = ResolveRasterAttachmentView(hasColor ? raster.colorView : raster.depth->view, color);
                if (const auto *error = std::get_if<FrameError>(&colorResolved))
                    return *error;
                const auto &colorView    = std::get<ResolvedRasterAttachmentView>(colorResolved);
                const auto &colorOffsets = imageOffsets[attachment.index];
                const Use   image{imageBase + attachment.index, {colorOffsets[colorView.subresource], colorOffsets[colorView.subresource + 1]}};
                const auto &extent = colorView.extent;
                if (hasColor && (!ColorFormat(color.format) || !color.usage.TestBit(ImageUsageFlagBit::RENDER_TARGET)))
                    return Invalid("Raster color requires a color render target");
                if (raster.colorResolve) {
                    const auto &resolve = *raster.colorResolve;
                    if (!ValidImage(resolve.image) || resolve.image == raster.color || color.samples == SampleCount::X1 ||
                        raster.load == LoadOp::DONT_CARE)
                        return Invalid("Invalid color resolve source/target or undefined source contents");
                    const auto &desc     = data.images[resolve.image.index].descriptor;
                    auto        resolved = ResolveRasterAttachmentView(resolve.view, desc);
                    if (const auto *error = std::get_if<FrameError>(&resolved))
                        return *error;
                    const auto &view = std::get<ResolvedRasterAttachmentView>(resolved);
                    if (desc.samples != SampleCount::X1 || desc.format != color.format || view.extent.width != extent.width ||
                        view.extent.height != extent.height || !desc.usage.TestBit(ImageUsageFlagBit::RENDER_TARGET))
                        return Invalid("Color resolve target must match format/extent and have one sample");
                    const auto &offsets = imageOffsets[resolve.image.index];
                    const Use   use{imageBase + resolve.image.index, {offsets[view.subresource], offsets[view.subresource + 1]}};
                    access.writes.push_back(use);
                    access.defines.push_back(use);
                }
                if (raster.depth) {
                    const auto &depth = *raster.depth;
                    if (!ValidImage(depth.image) || depth.load > LoadOp::CLEAR || depth.store > StoreOp::STORE || !std::isfinite(depth.clear) ||
                        depth.clear < 0.f || depth.clear > 1.f)
                        return Invalid("Invalid depth attachment");
                    const auto &desc          = data.images[depth.image.index].descriptor;
                    auto        depthResolved = ResolveRasterAttachmentView(depth.view, desc);
                    if (const auto *error = std::get_if<FrameError>(&depthResolved))
                        return *error;
                    const auto &view = std::get<ResolvedRasterAttachmentView>(depthResolved);
                    if (desc.format != PixelFormat::D32 || !desc.usage.TestBit(ImageUsageFlagBit::DEPTH_STENCIL) || desc.samples != color.samples ||
                        view.extent.width != extent.width || view.extent.height != extent.height)
                        return Invalid("Depth view does not match raster extent/format/sample count");
                    const auto &offsets = imageOffsets[depth.image.index];
                    const Use   use{imageBase + depth.image.index, {offsets[view.subresource], offsets[view.subresource + 1]}};
                    if (depth.load == LoadOp::LOAD)
                        access.reads.push_back(use);
                    access.writes.push_back(use);
                    if (depth.store == StoreOp::STORE && depth.load != LoadOp::DONT_CARE)
                        access.defines.push_back(use);
                    if (depth.resolve) {
                        const auto &resolve = *depth.resolve;
                        if (!ValidImage(resolve.image) || resolve.image == depth.image || resolve.image == raster.color ||
                            (raster.colorResolve && resolve.image == raster.colorResolve->image) || desc.samples == SampleCount::X1 ||
                            depth.load == LoadOp::DONT_CARE ||
                            (resolve.mode != DepthResolveMode::MINIMUM && resolve.mode != DepthResolveMode::MAXIMUM))
                            return Invalid("Invalid depth resolve source, target or mode");
                        const auto &target   = data.images[resolve.image.index].descriptor;
                        auto        resolved = ResolveRasterAttachmentView(resolve.view, target);
                        if (const auto *error = std::get_if<FrameError>(&resolved))
                            return *error;
                        const auto &targetView = std::get<ResolvedRasterAttachmentView>(resolved);
                        if (target.format != PixelFormat::D32 || target.samples != SampleCount::X1 || target.memory != MemoryType::GPU_ONLY ||
                            !target.usage.TestBit(ImageUsageFlagBit::DEPTH_STENCIL) || targetView.extent.width != extent.width ||
                            targetView.extent.height != extent.height)
                            return Invalid("Depth resolve target must match D32 extent and have one sample");
                        const auto &targetOffsets = imageOffsets[resolve.image.index];
                        const Use   targetUse{imageBase + resolve.image.index,
                                              {targetOffsets[targetView.subresource], targetOffsets[targetView.subresource + 1]}};
                        access.writes.push_back(targetUse);
                        access.defines.push_back(targetUse);
                    }
                }

                for (const auto &draw : raster.draws) {
                    if (auto error = CollectDraw(draw, raster, extent, access))
                        return error;
                }

                if (hasColor) {
                    if (raster.load == LoadOp::LOAD)
                        access.reads.push_back(image);
                    access.writes.push_back(image);
                    if (raster.store == StoreOp::STORE && raster.load != LoadOp::DONT_CARE)
                        access.defines.push_back(image);
                }

                return {};
            }

            std::optional<FrameError> CollectPassAccesses()
            {
                accesses.resize(data.passes.size());
                for (size_t i = 0; i < data.passes.size(); ++i) {
                    auto      &access = accesses[i];
                    const auto error  = std::visit(
                        [&](const auto &pass) {
                            access.sideEffect = pass.sideEffect;
                            return CollectAccess(pass, access);
                        },
                        data.passes[i]);
                    if (error)
                        return error;
                }
                return {};
            }
            std::optional<FrameError> CollectExports()
            {
                for (const auto &root : data.exports) {
                    if (!ValidBuffer(root.buffer, root.offset, root.size))
                        return Invalid("Invalid buffer export");
                    roots.push_back({root.buffer.index, {root.offset, root.offset + root.size}});
                }
                for (auto root : data.imageExports) {
                    if (!ValidImage(root))
                        return Invalid("Invalid image export");
                    roots.push_back({imageBase + root.index, {0, imageOffsets[root.index].back()}});
                }

                return {};
            }

            std::vector<bool> CullPasses() const
            {
                std::vector<bool>               live(data.passes.size(), false);
                std::vector<std::vector<Range>> needed(data.buffers.size() + data.images.size());
                for (auto root : roots)
                    Include(needed[root.resource], root.range);
                for (size_t i = accesses.size(); i-- > 0;) {
                    const auto &access = accesses[i];
                    live[i]            = access.sideEffect;
                    for (auto write : access.writes)
                        live[i] = live[i] || Overlaps(needed[write.resource], write.range);
                    if (!live[i])
                        continue;
                    for (auto write : access.writes)
                        Remove(needed[write.resource], write.range);
                    for (auto read : access.reads)
                        Include(needed[read.resource], read.range);
                }

                return live;
            }

            FrameGraphAnalysisResult ValidateContents(const std::vector<bool> &live)
            {
                std::vector<std::vector<Range>> defined(data.buffers.size() + data.images.size());
                std::vector<bool>               used(defined.size(), false);
                for (uint32_t i = 0; i < data.buffers.size(); ++i) {
                    if (data.buffers[i].initialized)
                        defined[i].push_back({0, data.buffers[i].size});
                    else
                        defined[i] = std::move(initialBuffers[i]);
                }
                for (uint32_t i = 0; i < data.images.size(); ++i)
                    defined[imageBase + i] = std::move(initialContents[i]);
                FrameGraphAnalysis result;
                result.bufferLifetimes.resize(data.buffers.size());
                result.imageLifetimes.resize(data.images.size());
                auto touch = [&](uint32_t resource, uint32_t pass) {
                    auto &lifetime = resource < imageBase ? result.bufferLifetimes[resource] : result.imageLifetimes[resource - imageBase];
                    lifetime.first = std::min(lifetime.first, pass);
                    lifetime.last  = std::max(lifetime.last, pass);
                    used[resource] = true;
                };
                for (uint32_t i = 0; i < accesses.size(); ++i) {
                    if (!live[i])
                        continue;
                    const auto &access = accesses[i];
                    for (auto read : access.reads) {
                        if (!Covered(defined[read.resource], read.range))
                            return Invalid("Pass reads undefined contents");
                        touch(read.resource, i);
                    }
                    for (auto write : access.writes) {
                        Remove(defined[write.resource], write.range);
                        touch(write.resource, i);
                    }
                    for (auto write : access.defines)
                        Include(defined[write.resource], write.range);
                    result.livePasses.push_back(i);
                }
                for (auto root : roots) {
                    if (!Covered(defined[root.resource], root.range))
                        return Invalid("Export reads undefined contents");
                    touch(root.resource, static_cast<uint32_t>(data.passes.size()));
                }
                for (uint32_t i = 0; i < data.buffers.size(); ++i)
                    if (used[i])
                        result.usedBuffers.push_back(i);
                for (uint32_t i = 0; i < data.images.size(); ++i)
                    if (used[imageBase + i])
                        result.usedImages.push_back(i);
                return result;
            }

            const FrozenFrameGraph            &graph;
            const FrameGraphData              &data;
            const uint32_t                     imageBase;
            std::vector<std::vector<Range>>    initialBuffers, initialContents;
            std::vector<std::vector<uint64_t>> imageOffsets;
            std::vector<Accesses>              accesses;
            std::vector<Use>                   roots;
            uint64_t                           copyRanges = 0;
        };
    } // namespace

    FrameGraphAnalysisResult AnalyzeFrameGraph(const FrozenFrameGraph &graph) noexcept
    {
        return FrameGraphAnalyzer(graph).Analyze();
    }

    std::variant<FrameError, ResolvedRasterAttachmentView> ResolveRasterAttachmentView(const RasterAttachmentView &view,
                                                                                       const Image::Descriptor    &image) noexcept
    {
        if (image.imageType != ImageType::IMAGE_2D || (!ColorFormat(image.format) && image.format != PixelFormat::D32) ||
            (image.samples != SampleCount::X1 &&
             (image.mipLevels != 1 || (image.samples != SampleCount::X2 && image.samples != SampleCount::X4 && image.samples != SampleCount::X8))))
            return FrameError{FrameErrorCode::UNSUPPORTED, "Raster attachments require supported RGBA8/D32 2D samples"};
        if (!image.extent.width || !image.extent.height || image.extent.depth != 1 || view.mip >= image.mipLevels || view.mip >= 32 ||
            view.layer >= image.arrayLayers || uint64_t(image.mipLevels) * image.arrayLayers > MAX_FRAME_IMAGE_SUBRESOURCES)
            return Invalid("Invalid raster attachment view range");
        return ResolvedRasterAttachmentView{{view.mip, 1, view.layer, 1},
                                            {std::max(1u, image.extent.width >> view.mip), std::max(1u, image.extent.height >> view.mip)},
                                            view.layer * image.mipLevels + view.mip};
    }
    std::variant<FrameError, RasterImageView> ResolveRasterImageView(const RasterImageView &input, const Image::Descriptor &image) noexcept
    {
        if ((input.type != ImageViewType::VIEW_2D && input.type != ImageViewType::VIEW_2D_ARRAY) || image.imageType != ImageType::IMAGE_2D ||
            (image.format != PixelFormat::RGBA8_UNORM && image.format != PixelFormat::RGBA8_SRGB && image.format != PixelFormat::RGBA16_SFLOAT &&
             image.format != PixelFormat::D32) ||
            image.samples != SampleCount::X1 || !image.usage.TestBit(ImageUsageFlagBit::SAMPLED))
            return FrameError{FrameErrorCode::UNSUPPORTED, "Raster sampling requires RGBA8/RGBA16_FLOAT/D32 2D/2D-array views"};
        return NormalizeImageView(input, image, "Invalid sampled image view range");
    }
    bool IsRasterResourceSupported(const ShaderResource &resource) noexcept
    {
        if (resource.count != 1 || resource.descriptorArray || resource.set > 2 || resource.binding >= 32)
            return false;
        switch (resource.type) {
        case ShaderResourceType::UNIFORM_BUFFER:
        case ShaderResourceType::UNIFORM_BUFFER_DYNAMIC: return true;
        case ShaderResourceType::SAMPLER: return resource.sampler == ShaderSamplerType::REGULAR;
        case ShaderResourceType::SAMPLED_IMAGE:
            return resource.texture.dimension == ShaderTextureDimension::D2 && !resource.texture.multisampled &&
                   resource.texture.scalarType == ShaderScalarType::FLOAT && (resource.texture.components == 4 || resource.texture.components == 1);
        default: return false;
        }
    }

    std::optional<FrameError> ValidateRasterBindings(const RasterDraw &draw, const ShaderReflection &reflection) noexcept
    {
        if (!reflection.pushConstants.empty())
            return FrameError{FrameErrorCode::UNSUPPORTED, "Raster push constants are not implemented"};
        if (reflection.resources.size() != draw.uniforms.size() + draw.images.size() + draw.samplers.size())
            return Invalid("Raster bindings do not match shader reflection");
        const FrameBindingLookup<RasterUniformBinding> uniforms(draw.uniforms);
        const FrameBindingLookup<RasterImageBinding>   images(draw.images);
        const FrameBindingLookup<RasterSamplerBinding> samplers(draw.samplers);
        for (size_t i = 0; i < reflection.resources.size(); ++i) {
            const auto &resource = reflection.resources[i];
            if (!IsRasterResourceSupported(resource))
                return FrameError{FrameErrorCode::UNSUPPORTED, "Unsupported raster descriptor shape/type/count/set"};
            for (size_t j = 0; j < i; ++j)
                if (reflection.resources[j].set == resource.set && reflection.resources[j].binding == resource.binding)
                    return Invalid("Duplicate reflected binding");
            if (resource.type == ShaderResourceType::SAMPLED_IMAGE) {
                const auto *binding = images.Find(resource.set, resource.binding);
                if (!binding || resource.texture.arrayed != (binding->view.type == ImageViewType::VIEW_2D_ARRAY))
                    return Invalid("Missing sampled image binding or shader/view dimension mismatch");
                continue;
            }
            if (resource.type == ShaderResourceType::SAMPLER) {
                if (!samplers.Find(resource.set, resource.binding))
                    return Invalid("Missing sampler binding");
                continue;
            }
            const auto *found = uniforms.Find(resource.set, resource.binding);
            if (!found || (resource.type == ShaderResourceType::UNIFORM_BUFFER && found->dynamicOffset))
                return Invalid("Missing uniform binding or unexpected dynamic offset");
        }
        for (const auto &block : reflection.blocks) {
            const auto *found = uniforms.Find(block.set, block.binding);
            if (!found || !block.size || block.size > found->range.size)
                return Invalid("Uniform range does not cover reflected block");
        }
        return {};
    }
    bool RasterPipelineLess(const RasterDraw &a, const RasterDraw &b) noexcept
    {
        auto blend = [](const BlendState &s) {
            return std::tuple{s.blendEn, s.writeMask, s.srcColor, s.dstColor, s.srcAlpha, s.dstAlpha, s.colorBlendOp, s.alphaBlendOp};
        };
        if (a.shader.Get() != b.shader.Get())
            return std::less<const Shader *>{}(a.shader.Get(), b.shader.Get());
        if (blend(a.blend) != blend(b.blend))
            return blend(a.blend) < blend(b.blend);
        if (a.depth != b.depth)
            return std::tie(a.depth.test, a.depth.write, a.depth.compare) < std::tie(b.depth.test, b.depth.write, b.depth.compare);
        if (a.attributes != b.attributes)
            return std::lexicographical_compare(a.attributes.begin(), a.attributes.end(), b.attributes.begin(), b.attributes.end(),
                                                [](const auto &x, const auto &y) {
                                                    return std::tie(x.location, x.binding, x.offset, x.format, x.semantic, x.semanticIndex) <
                                                           std::tie(y.location, y.binding, y.offset, y.format, y.semantic, y.semanticIndex);
                                                });
        if (a.vertices.size() != b.vertices.size())
            return a.vertices.size() < b.vertices.size();
        for (size_t i = 0; i < a.vertices.size(); ++i)
            if (a.vertices[i].stride != b.vertices[i].stride || a.vertices[i].rate != b.vertices[i].rate)
                return std::tie(a.vertices[i].stride, a.vertices[i].rate) < std::tie(b.vertices[i].stride, b.vertices[i].rate);
        return false;
    }
    PipelineState RasterPipelineState(const RasterDraw &draw)
    {
        PipelineState result;
        result.blendStates                 = {draw.blend};
        result.rasterState.depthClipEnable = true;
        result.depthStencil.depthTest      = draw.depth.test;
        result.depthStencil.depthWrite     = draw.depth.write;
        result.depthStencil.compareOp      = draw.depth.compare;
        for (uint32_t i = 0; i < draw.vertices.size(); ++i)
            result.vertexBindings.push_back({i, draw.vertices[i].stride, draw.vertices[i].rate});
        for (const auto &attr : draw.attributes)
            result.vertexAttributes.push_back({attr.location, attr.binding, attr.offset, attr.format, attr.semantic.c_str(), attr.semanticIndex});
        return result;
    }
} // namespace sky::aurora

//
// Created on 2026/10/16.
//

#include "FrameTransientPool.h"

#include <algorithm>
#include <aurora/rhi/Device.h>

namespace sky::aurora {
    namespace {
        template <typename Resource, typename Compatible>
        std::vector<uint32_t> AssignBacking(std::vector<uint32_t>                     used,
                                            const std::vector<FrameResourceLifetime> &lifetimes,
                                            const std::vector<Resource>              &resources,
                                            Compatible                                compatible)
        {
            std::stable_sort(used.begin(), used.end(), [&](uint32_t a, uint32_t b) { return lifetimes[a].first < lifetimes[b].first; });
            std::vector<uint32_t> mapping(lifetimes.size(), UINT32_MAX), last(lifetimes.size()), roots;
            for (auto index : used) {
                mapping[index] = index;
                if (!resources[index].transient)
                    continue;
                const auto found = std::find_if(roots.begin(), roots.end(),
                                                [&](uint32_t root) { return last[root] < lifetimes[index].first && compatible(root, index); });
                const auto root  = found == roots.end() ? index : *found;
                if (root == index)
                    roots.push_back(root);
                mapping[index] = root;
                last[root]     = lifetimes[index].last;
            }
            return mapping;
        }
    } // namespace
    bool FrameTransientPool::MakeRoom(uint64_t bytes)
    {
        if (!limits.objects || bytes > limits.bytes)
            return false;
        while (entries.size() >= limits.objects || bytes > limits.bytes - stats.bytes) {
            const auto idle = std::find_if(entries.begin(), entries.end(), [](const Entry &entry) { return entry.borrower.expired(); });
            if (idle == entries.end())
                return false;
            stats.bytes -= idle->bytes;
            entries.erase(idle);
            ++stats.evictions;
        }
        return true;
    }
    std::variant<FrameError, FrameResourceBindings>
    FrameTransientPool::Acquire(const FrameGraphData &data, const FrameGraphAnalysis &analysis, uint64_t epoch)
    {
        if (!epoch)
            return FrameError{FrameErrorCode::INVALID_ARGUMENT};
        FrameResourceBindings bindings;
        bindings.payload  = data.payload;
        auto retainShader = [&](const ShaderPtr &shader, const FrameBindingGroups &groups) {
            bindings.objects.emplace_back(shader.Get());
            for (const auto &use : groups)
                if (use.group)
                    for (const auto &sampler : use.group->GetData().samplers)
                        bindings.objects.emplace_back(sampler.sampler.Get());
        };
        for (auto index : analysis.livePasses) {
            if (const auto *raster = std::get_if<FrameRasterPass>(&data.passes[index]))
                for (const auto &draw : raster->draws)
                    retainShader(draw.shader, draw.groups);
            if (const auto *compute = std::get_if<FrameComputePass>(&data.passes[index]))
                retainShader(compute->shader, compute->groups);
        }
        bindings.buffers.resize(data.buffers.size());
        bindings.images.resize(data.images.size());
        bindings.bufferBacking = AssignBacking(analysis.usedBuffers, analysis.bufferLifetimes, data.buffers, [&](uint32_t a, uint32_t b) {
            const auto &x = data.buffers[a].transient;
            const auto &y = data.buffers[b].transient;
            return x && y && x->size == y->size && x->usage == y->usage && x->memory == y->memory;
        });
        bindings.imageBacking  = AssignBacking(analysis.usedImages, analysis.imageLifetimes, data.images, [&](uint32_t a, uint32_t b) {
            return data.images[a].transient && data.images[b].transient && IsSameImageStorage(data.images[a].descriptor, data.images[b].descriptor);
        });
        std::lock_guard lock(mutex);
        auto            borrow = [&](Entry &entry) {
            if (!bindings.transientLease)
                bindings.transientLease = std::make_shared<const uint8_t>(0);
            entry.borrower = bindings.transientLease;
        };
        for (auto index : analysis.usedBuffers) {
            if (bindings.bufferBacking[index] != index)
                continue;
            const auto &input = data.buffers[index];
            if (!input.transient) {
                bindings.buffers[index] = input.buffer;
                continue;
            }
            auto desc = *input.transient;
#if SKY_ENABLE_RESOURCE_NAME
            desc.name = nullptr;
#endif
            const auto found = std::find_if(entries.begin(), entries.end(), [&](const Entry &entry) {
                return entry.buffer && entry.epoch == epoch && entry.borrower.expired() && entry.bufferDescriptor.size == desc.size &&
                       entry.bufferDescriptor.usage == desc.usage && entry.bufferDescriptor.memory == desc.memory;
            });
            if (found != entries.end()) {
                borrow(*found);
                bindings.buffers[index] = found->buffer;
                ++stats.reuses;
                continue;
            }
            if (!MakeRoom(desc.size))
                return FrameError{FrameErrorCode::OUT_OF_MEMORY, "Transient pool budget exhausted"};
            Entry entry;
            entry.buffer = BufferPtr(device.CreateBuffer(desc));
            if (!entry.buffer)
                return FrameError{FrameErrorCode::COMPILE_FAILED, "Transient buffer allocation failed"};
            entry.bufferDescriptor = desc;
            entry.bytes            = desc.size;
            entry.epoch            = epoch;
            borrow(entry);
            bindings.buffers[index] = entry.buffer;
            stats.bytes += entry.bytes;
            ++stats.allocations;
            entries.push_back(std::move(entry));
        }
        for (auto index : analysis.usedImages) {
            if (bindings.imageBacking[index] != index)
                continue;
            const auto &input = data.images[index];
            if (!input.transient) {
                bindings.images[index] = input.image;
                continue;
            }
            auto desc = input.descriptor;
#if SKY_ENABLE_RESOURCE_NAME
            desc.name = nullptr;
#endif
            const auto found = std::find_if(entries.begin(), entries.end(), [&](const Entry &entry) {
                return entry.image && entry.epoch == epoch && entry.borrower.expired() && IsSameImageStorage(entry.imageDescriptor, desc);
            });
            if (found != entries.end()) {
                borrow(*found);
                bindings.images[index] = found->image;
                ++stats.reuses;
                continue;
            }
            // Logical texel bytes bound this cache; native allocation padding is backend-dependent.
            uint64_t       bytes      = 0;
            const uint64_t copies     = uint64_t(desc.arrayLayers) * static_cast<uint32_t>(desc.samples);
            const uint32_t texelBytes = desc.format == PixelFormat::RGBA16_SFLOAT ? 8 : 4;
            for (uint32_t mip = 0; mip < desc.mipLevels; ++mip) {
                const uint64_t texels = uint64_t(std::max(1u, desc.extent.width >> mip)) * std::max(1u, desc.extent.height >> mip);
                if (texels > (limits.bytes - bytes) / copies / texelBytes)
                    return FrameError{FrameErrorCode::OUT_OF_MEMORY, "Transient image exceeds pool budget"};
                bytes += texels * texelBytes * copies;
            }
            if (!MakeRoom(bytes))
                return FrameError{FrameErrorCode::OUT_OF_MEMORY, "Transient pool budget exhausted"};
            Entry entry;
            entry.image = ImagePtr(device.CreateImage(desc));
            if (!entry.image)
                return FrameError{FrameErrorCode::COMPILE_FAILED, "Transient image allocation failed"};
            entry.imageDescriptor = desc;
            entry.bytes           = bytes;
            entry.epoch           = epoch;
            borrow(entry);
            bindings.images[index] = entry.image;
            stats.bytes += bytes;
            ++stats.allocations;
            entries.push_back(std::move(entry));
        }
        for (auto index : analysis.usedBuffers)
            if (bindings.bufferBacking[index] != index) {
                bindings.buffers[index] = bindings.buffers[bindings.bufferBacking[index]];
                ++stats.aliases;
            }
        for (auto index : analysis.usedImages)
            if (bindings.imageBacking[index] != index) {
                bindings.images[index] = bindings.images[bindings.imageBacking[index]];
                ++stats.aliases;
            }
        return bindings;
    }
    FrameTransientPoolStats FrameTransientPool::GetStats() const
    {
        std::lock_guard lock(mutex);
        auto            result = stats;
        result.objects         = static_cast<uint32_t>(entries.size());
        result.borrowed =
            static_cast<uint32_t>(std::count_if(entries.begin(), entries.end(), [](const Entry &entry) { return !entry.borrower.expired(); }));
        return result;
    }
} // namespace sky::aurora

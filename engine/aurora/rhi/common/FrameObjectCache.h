#pragma once

#include <aurora/rdg/FrozenFrameGraph.h>
#include <aurora/rhi/PipelineState.h>
#include <list>
#include <mutex>
#include <tuple>

namespace sky::aurora {
    template <class Key, class Value>
    class FrameObjectCache final {
    public:
        explicit FrameObjectCache(size_t capacity = 128) : capacity(capacity)
        {
        }
        template <class Factory>
        std::variant<FrameError, Value> Acquire(Key key, Factory &&factory)
        {
            std::lock_guard lock(mutex);
            for (auto it = entries.begin(); it != entries.end(); ++it) {
                if (it->key == key) {
                    entries.splice(entries.begin(), entries, it);
                    return entries.front().value;
                }
            }
            auto result = factory();
            if (auto *value = std::get_if<Value>(&result); value && capacity) {
                entries.push_front({std::move(key), *value});
                if (entries.size() > capacity)
                    entries.pop_back();
            }
            return result;
        }
        void Clear()
        {
            std::lock_guard lock(mutex);
            entries.clear();
        }

    private:
        struct Entry {
            Key   key;
            Value value;
        };
        const size_t     capacity;
        std::mutex       mutex;
        std::list<Entry> entries;
    };

    struct FramePipelineKey {
        FramePipelineKey(const RasterDraw &source, const AttachmentFormat &format, uint64_t epoch, uint64_t scope = 0, uint32_t subpass = 0)
            : format(format), epoch(epoch), scope(scope), subpass(subpass)
        {
            draw.shader     = source.shader;
            draw.blend      = source.blend;
            draw.depth      = source.depth;
            draw.attributes = source.attributes;
            for (const auto &stream : source.vertices)
                draw.vertices.push_back({{}, stream.stride, stream.rate});
        }
        bool operator==(const FramePipelineKey &other) const
        {
            if (std::tie(epoch, scope, subpass, format.numColors, format.depthStencil, format.sampleCount, format.viewMask) !=
                std::tie(other.epoch, other.scope, other.subpass, other.format.numColors, other.format.depthStencil, other.format.sampleCount,
                         other.format.viewMask))
                return false;
            for (uint32_t i = 0; i < format.numColors; ++i)
                if (format.colors[i] != other.format.colors[i])
                    return false;
            return !RasterPipelineLess(draw, other.draw) && !RasterPipelineLess(other.draw, draw);
        }
        // Only immutable pipeline inputs are retained; graph ranges and per-frame descriptors never enter the cache.
        RasterDraw       draw;
        AttachmentFormat format;
        uint64_t         epoch, scope;
        uint32_t         subpass;
    };
    using FramePipelineCache = FrameObjectCache<FramePipelineKey, CounterPtr<GraphicsPipeline>>;
} // namespace sky::aurora

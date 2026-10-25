#pragma once

#include <aurora/rdg/FrozenFrameGraph.h>
#include <functional>
#include <tuple>

namespace sky::aurora {
    // Keys borrow immutable draws within one pass; descriptors never cross a frame lease or attachment scope.
    struct FrameRasterBindingsLess {
        bool operator()(const RasterDraw *a, const RasterDraw *b) const
        {
            if (a->shader.Get() != b->shader.Get())
                return std::less<const Shader *>{}(a->shader.Get(), b->shader.Get());
            if (a->uniforms.size() != b->uniforms.size())
                return a->uniforms.size() < b->uniforms.size();
            for (size_t i = 0; i < a->uniforms.size(); ++i) {
                const auto key = [](const RasterUniformBinding &v) {
                    return std::tie(v.set, v.binding, v.range.buffer.graph, v.range.buffer.index, v.range.offset, v.range.size, v.dynamicOffset);
                };
                if (key(a->uniforms[i]) != key(b->uniforms[i]))
                    return key(a->uniforms[i]) < key(b->uniforms[i]);
            }
            if (a->storage.size() != b->storage.size()) { return a->storage.size() < b->storage.size(); }
            for (size_t i = 0; i < a->storage.size(); ++i) {
                const auto key = [](const RasterUniformBinding &v) { return std::tie(v.set, v.binding, v.range.buffer.graph, v.range.buffer.index, v.range.offset, v.range.size, v.dynamicOffset); };
                if (key(a->storage[i]) != key(b->storage[i])) { return key(a->storage[i]) < key(b->storage[i]); }
            }
            if (a->images.size() != b->images.size())
                return a->images.size() < b->images.size();
            for (size_t i = 0; i < a->images.size(); ++i) {
                const auto key = [](const RasterImageBinding &v) {
                    return std::tie(v.set, v.binding, v.image.graph, v.image.index, v.view.type, v.view.baseMip, v.view.mipCount, v.view.baseLayer,
                                    v.view.layerCount);
                };
                if (key(a->images[i]) != key(b->images[i]))
                    return key(a->images[i]) < key(b->images[i]);
            }
            if (a->samplers.size() != b->samplers.size())
                return a->samplers.size() < b->samplers.size();
            for (size_t i = 0; i < a->samplers.size(); ++i) {
                const auto &x = a->samplers[i], &y = b->samplers[i];
                if (std::tie(x.set, x.binding) != std::tie(y.set, y.binding))
                    return std::tie(x.set, x.binding) < std::tie(y.set, y.binding);
                if (x.sampler.Get() != y.sampler.Get())
                    return std::less<const Sampler *>{}(x.sampler.Get(), y.sampler.Get());
            }
            return false;
        }
    };
} // namespace sky::aurora

//
// Created on 2026/10/27.
//

#pragma once

#include <aurora/rdg/FrozenFrameGraph.h>

namespace sky::aurora {

    struct RasterBindingDraft {
        std::vector<RasterUniformBinding> uniforms;
        std::vector<RasterUniformBinding> storage;
        std::vector<RasterImageBinding>   images;
        std::vector<RasterSamplerBinding> samplers;
    };
    struct ComputeBindingDraft {
        std::vector<FrameBufferBinding>   buffers;
        std::vector<FrameImageBinding>    images;
        std::vector<RasterSamplerBinding> samplers;
    };
    struct FrameBindingGroupLess {
        bool operator()(const FrameBindingGroup *, const FrameBindingGroup *) const noexcept;
    };

    FrameOperationResult      SetRasterBindings(RasterDraw &, RasterBindingDraft);
    FrameOperationResult      SetComputeBindings(FrameComputePass &, ComputeBindingDraft);
    std::optional<FrameError> ValidateBindingGroups(const Shader *, const FrameBindingGroups &) noexcept;
    // Resolved values are compilation-local; validate group shape before using them.
    RasterBindingDraft  ResolveRasterBindings(const RasterDraw &);
    ComputeBindingDraft ResolveComputeBindings(const FrameComputePass &);

} // namespace sky::aurora

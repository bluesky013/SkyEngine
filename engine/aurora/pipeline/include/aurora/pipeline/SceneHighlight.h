//
// Created on 2026/10/23.
//

#pragma once

#include <aurora/renderer/RenderCommands.h>
#include <span>

namespace sky::aurora {
    struct SceneHighlightStyle {
        Vector4 visible{1.f, .55f, .08f, 1.f};
        Vector4 occluded{1.f, .55f, .08f, .25f};
        float   width = 2.f;
    };
    // Owned per-output inputs; empty entity lists add no highlight passes.
    FrameExtensionPayload
    SceneHighlightInput(Name feature, ViewportId output, std::span<const RenderEntityRef> entities, const SceneHighlightStyle &style = {});
} // namespace sky::aurora

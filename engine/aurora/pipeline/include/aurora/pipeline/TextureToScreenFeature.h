#pragma once

#include <aurora/renderer/IRenderFeature.h>

namespace sky::aurora {
    std::shared_ptr<IFrameGraphFeature> CreateTextureToScreenFeature(Name name, RenderTechniqueData shader);
    FrameExtensionPayload               TextureToScreenInput(Name feature, ViewportId output, ViewOutputRef source, Vector4 tint = {1, 1, 1, 1});
    FrameExtensionPayload               TextureToScreenInput(Name feature, ViewportId output, RenderResourceKey source, Vector4 tint = {1, 1, 1, 1});
} // namespace sky::aurora

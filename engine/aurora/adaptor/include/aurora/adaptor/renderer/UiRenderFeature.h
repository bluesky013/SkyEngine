//
// Created on 2026/10/17.
//

#pragma once

#include <array>
#include <aurora/renderer/IRenderFeature.h>
#include <string_view>

namespace sky::aurora {
    struct RenderUiVertex {
        float                x = 0, y = 0, u = 0, v = 0;
        std::array<float, 4> color{1, 1, 1, 1};
        std::array<float, 4> shape{};
        float                radius = 0;
    };
    struct RenderUiDraw {
        ViewOutputRef texture;
        uint32_t      firstIndex = 0, indexCount = 0;
        // Top-left normalized clipping; coordinates are transformed using the actual output.
        std::array<float, 4> clip{0, 0, 1, 1};
        RenderResourceKey    image;
    };
    struct RenderUiSnapshot {
        ViewportId                  output;
        std::vector<RenderUiVertex> vertices;
        std::vector<uint32_t>       indices;
        std::vector<RenderUiDraw>   draws;
    };
    std::variant<RendererError, FrameExtensionPayload> EncodeRenderUiSnapshot(Name, const RenderUiSnapshot &);
    std::string_view                                   RenderUiShaderSource();
    // Compile the supplied source on the producer; the feature creates native objects on the coordinator.
    std::variant<RendererError, std::shared_ptr<IFrameGraphFeature>> CreateUiRenderFeature(Name, RenderMaterialData);
} // namespace sky::aurora

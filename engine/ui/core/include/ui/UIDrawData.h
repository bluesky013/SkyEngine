//
// Created on 2026/09/19.
//

#pragma once

#include <ui/UIRect.h>

#include <cstdint>
#include <vector>

namespace sky::ui {

    using UITextureId = uint32_t;
    constexpr UITextureId UI_INVALID_TEXTURE = 0;

    struct UIVertex {
        float x = 0.0f;
        float y = 0.0f;
        float u = 0.0f;
        float v = 0.0f;
        uint32_t color = 0xFFFFFFFF;
    };

    struct UIDrawCmd {
        uint32_t indexOffset = 0;
        uint32_t indexCount = 0;
        UIRect clip;
        UITextureId textureId = UI_INVALID_TEXTURE;
    };

    struct UIDrawData {
        std::vector<UIVertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<UIDrawCmd> commands;

        void Clear()
        {
            vertices.clear();
            indices.clear();
            commands.clear();
        }
    };

} // namespace sky::ui

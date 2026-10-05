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
        // Rounded-box SDF parameters: rect center (xy), half extents (zw), radius.
        float roundCenterX = 0.0f;
        float roundCenterY = 0.0f;
        float roundHalfX = 0.0f;
        float roundHalfY = 0.0f;
        float roundRadius = 0.0f;
    };

    struct UIDrawCmd {
        uint32_t indexOffset = 0;
        uint32_t indexCount = 0;
        UIRect clip;
        UITextureId textureId = UI_INVALID_TEXTURE;
        bool shape = false; // draw as a rounded box (fs_round), ignores the texture
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

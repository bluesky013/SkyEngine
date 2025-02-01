//
// Created by blues on 2024/9/19.
//

#pragma once

#include <render/text/Text.h>
#include <render/text/Font.h>

namespace sky {
    class RenderScene;

    class RenderProfiler {
    public:
        explicit RenderProfiler(RenderScene *scn);
        ~RenderProfiler();

        void SetDisplaySize(uint32_t w, uint32_t h);
    private:
        void UpdateText();

        RenderScene* scene = nullptr;
        Text* text         = nullptr;

        uint32_t displayWidth  = 1;
        uint32_t displayHeight = 1;
        FontPtr font;
    };

} // namespace sky

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
        RenderScene* scene = nullptr;
        Text* text         = nullptr;

        FontPtr font;
    };

} // namespace sky


//
// Created by Zach Lee on 2021/11/14.
//


#pragma once

namespace sky {

    class Render;
    class RenderPipeline;

    class RenderScene {
    public:
        RenderScene() = default;
        ~RenderScene() = default;

        void OnTick(float time);
    private:
    };

}
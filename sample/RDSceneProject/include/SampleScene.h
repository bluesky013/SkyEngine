//
// Created by Zach Lee on 2023/9/2.
//

#pragma once

namespace sky {
    class RenderScene;
    class RenderWindow;

    class SampleScene {
    public:
        SampleScene() = default;
        virtual ~SampleScene() = default;

        virtual bool Start(RenderWindow *window);
        virtual void Shutdown();

        virtual void Tick(float time) {}

    protected:
        RenderScene *scene = nullptr;
    };

} // namespace sky
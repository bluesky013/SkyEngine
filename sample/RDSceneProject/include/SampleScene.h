//
// Created by Zach Lee on 2023/9/2.
//

#pragma once

#include <memory>
#include <framework/world/World.h>
#include <render/adaptor/RenderSceneProxy.h>

namespace sky {
    class RenderScene;
    class RenderWindow;
    class World;

    class SampleScene {
    public:
        SampleScene() = default;
        virtual ~SampleScene() = default;

        virtual bool Start(RenderWindow *window);
        virtual void Shutdown();

        virtual void Tick(float time);

    protected:
        std::unique_ptr<World> world;
        std::unique_ptr<RenderSceneProxy> sceneProxy;
    };

} // namespace sky

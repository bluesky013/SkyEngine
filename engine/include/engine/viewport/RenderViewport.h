//
// Created by Zach Lee on 2023/1/22.
//

#pragma once

#include <memory>
#include <framework/window/IWindow.h>
#include <engine/world/World.h>

namespace sky {

    class RenderViewport {
    public:
        RenderViewport() = default;
        ~RenderViewport() = default;

        void SetWindow(IWindow *window_) { window = window_; }
        void SetWorld(const WorldPtr &world_) { world = world_; }

    private:
        IWindow *window = nullptr;
        WorldPtr world;
    };

} // namespace sky

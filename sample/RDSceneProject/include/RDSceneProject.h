//
// Created by Zach Lee on 2022/8/13.
//

#pragma once

#include <framework/window/IWindowEvent.h>
#include <framework/interface/IModule.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>

namespace sky {
    class RenderScene;
    class RenderWindow;

    class RDSceneProject : public IModule, public IWindowEvent {
    public:
        RDSceneProject()  = default;
        ~RDSceneProject() override = default;

        bool Init() override;
        void Start() override;
        void Stop() override;
        void Tick(float delta) override;

        void OnWindowResize(uint32_t width, uint32_t height) override;

    private:
        RenderScene *scene = nullptr;
        RenderWindow *window = nullptr;
    };

} // namespace sky
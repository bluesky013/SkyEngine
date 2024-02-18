//
// Created by Zach Lee on 2022/8/13.
//

#pragma once

#include "SampleScene.h"
#include "framework/interface/IModule.h"
#include "framework/interface/ISystem.h"
#include "framework/interface/Interface.h"
#include "framework/window/IWindowEvent.h"
#include <unordered_map>

namespace sky {
    class RenderScene;
    class RenderWindow;

    class RDSceneProject : public IModule, public IWindowEvent {
    public:
        RDSceneProject()  = default;
        ~RDSceneProject() override = default;

        bool Init(const StartArguments &args) override;
        void Shutdown() override;
        void Start() override;

        void Tick(float delta) override;

        void OnWindowResize(uint32_t width, uint32_t height) override;
        void OnKeyUp(KeyButtonType) override;
    private:
        void NextScene();

        RenderWindow *window = nullptr;
        SampleScene *currentScene = nullptr;
        uint32_t sceneIndex = 0;
        std::vector<std::unique_ptr<SampleScene>> sampleScenes;
    };

} // namespace sky
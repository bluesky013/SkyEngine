//
// Created by Zach Lee on 2022/8/13.
//

#pragma once

#include <framework/interface/IModule.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>

#include <render/RenderCamera.h>
#include <render/RenderScene.h>
#include <render/RenderViewport.h>

namespace sky {
    class RDSceneProject : public IModule {
    public:
        RDSceneProject()  = default;
        ~RDSceneProject() = default;

        void Init() override;

        void Start() override;

        void Stop() override;

        void Tick(float delta) override;

    private:
        RDScenePtr    scene;
        RenderCamera *mainCamera = nullptr;
    };

} // namespace sky
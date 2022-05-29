//
// Created by Zach Lee on 2021/11/12.
//


#pragma once
#include <render/RenderScene.h>
#include <render/RenderView.h>
#include <core/environment/Singleton.h>
#include <framework/interface/IEngine.h>

namespace sky {

    namespace drv {
        class Driver;
        class Device;
    }

    class RenderScene;

    class Render : public Singleton<Render> {
    public:
        bool Init(const StartInfo&);

        void OnTick(float time);

        void AddScene(RDScenePtr scene);

    private:
        friend class Singleton<Render>;
        Render() = default;
        ~Render();

        std::vector<RDScenePtr> scenes;
    };

}
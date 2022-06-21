
//
// Created by Zach Lee on 2021/11/12.
//

#include <render/Render.h>
#include <render/DriverManager.h>
#include <render/DevObjManager.h>

#include <core/logger/Logger.h>

static const char* TAG = "Render";

namespace sky {

    Render::~Render()
    {
        DevObjManager::Get()->Destroy();
        DriverManager::Get()->Destroy();
    }

    bool Render::Init(const StartInfo& info)
    {
        LOG_I(TAG, "Init Render");
        if (!DriverManager::Get()->Initialize({info.appName})) {
            LOG_E(TAG, "Init Driver Failed");
            return false;
        }
        return true;
    }

    void Render::OnTick(float time)
    {
        for (auto& scene : scenes) {
            scene->OnPreRender();
        }

        for (auto& scene : scenes) {
            scene->OnRender();
        }

        for (auto& scene : scenes) {
            scene->OnPostRender();
        }
    }

    void Render::AddScene(RDScenePtr scene)
    {
        scenes.emplace_back(scene);
    }

//    void Render::OnWindowResize(void* hwnd, uint32_t w, uint32_t h)
//    {
//        auto iter = swapChains.find(hwnd);
//        if (iter == swapChains.end()) {
//            return;
//        }
//        iter->second->Resize(w, h);
//    }
}
//
// Created by Zach Lee on 2023/8/17.
//

#pragma once

#include <memory>
#include <core/environment/Singleton.h>

#include <rhi/Device.h>
#include <rhi/Instance.h>

#include <render/RenderScene.h>
#include <render/RenderWindow.h>

#include <render/rdg/RenderGraphContext.h>

namespace sky {

    class Renderer : public Singleton<Renderer> {
    public:
        Renderer();
        ~Renderer() override;

        void Init();
        void Tick(float time);

        RenderScene *CreateScene();
        void RemoveScene(RenderScene *scene);

        RenderWindow *CreateRenderWindow();
        void DestroyRenderWindow(RenderWindow *);

    private:
        template <typename T>
        static void DestroyObj(T *ptr)
        {
            delete ptr;
        }

        void BeforeRender(float time);
        void Render();
        void AfterRender(float time);

        rhi::Device *device = nullptr;

        PmrUnSyncPoolRes mainPool;

        std::unique_ptr<rdg::RenderGraphContext> rdgContext;
        PmrList<std::unique_ptr<RenderScene, decltype(&Renderer::DestroyObj<RenderScene>)>> scenes;
        PmrList<std::unique_ptr<RenderWindow, decltype(&Renderer::DestroyObj<RenderWindow>)>> windows;
    };
} // namespace sky
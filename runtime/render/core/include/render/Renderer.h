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
#include <render/RenderBase.h>

#include <render/rdg/RenderGraphContext.h>
#include <render/RenderDefaultResource.h>
#include <render/RenderResourceGC.h>
#include <render/FeatureProcessor.h>
#include <render/RenderStreamManager.h>
#include <render/RenderPipeline.h>
#include <render/resource/MaterialManager.h>

namespace sky {

    class Renderer : public Singleton<Renderer> {
    public:
        Renderer();
        ~Renderer() override;

        void Init();
        void Tick(float time);

        void StopRender();

        RenderScene *CreateScene();
        void RemoveScene(RenderScene *scene);

        RenderWindow *CreateRenderWindow(void *hWnd, uint32_t width, uint32_t height, bool vSync);
#ifdef SKY_ENABLE_XR
        RenderWindow *CreateRenderWindowByXR();
#endif

        void DestroyRenderWindow(RenderWindow *);

        uint32_t GetInflightFrameCount() const { return inflightFrameCount; }
        RenderResourceGC *GetResourceGC() const;
        RenderStreamManager *GetStreamingManager() const { return streamManager.get(); }
        MaterialManager *GetMaterialManager() const { return materialManager.get(); }

        const RenderDefaultResource &GetDefaultResource() const { return defaultResource; }

        void SetCacheFolder(const std::string &path) { cacheFolder = path; }
        const std::string &GetCacheFolder() const { return cacheFolder; }

        void SetShaderCompiler(ShaderCompileFunc func) { shaderCompiler = func; }
        ShaderCompileFunc GetShaderCompiler() const { return shaderCompiler; }

        void SetPipeline(RenderPipeline *pipeline);

        template <typename T>
        void RegisterRenderFeature()
        {
            features.emplace_back(new FeatureProcessorBuilder<T>());
        }
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

        uint32_t totalFrame = 0;
        uint32_t inflightFrameCount = 2;
        uint32_t frameIndex = 0;
        RenderDefaultResource defaultResource;

        PmrList<std::unique_ptr<RenderScene, decltype(&Renderer::DestroyObj<RenderScene>)>> scenes;
        PmrList<std::unique_ptr<RenderWindow, decltype(&Renderer::DestroyObj<RenderWindow>)>> windows;
        PmrVector<std::unique_ptr<RenderResourceGC>> delayReleaseCollections;
        PmrVector<std::unique_ptr<IFeatureProcessorBuilder>> features;

        std::unique_ptr<RenderStreamManager> streamManager;
        std::unique_ptr<MaterialManager> materialManager;
        std::unique_ptr<RenderPipeline> pipeline;

        ShaderCompileFunc shaderCompiler = nullptr;
        std::string cacheFolder;
    };
} // namespace sky

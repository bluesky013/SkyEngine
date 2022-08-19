//
// Created by Zach Lee on 2021/11/12.
//


#pragma once
#include <render/RenderScene.h>
#include <render/RenderView.h>
#include <render/RenderViewport.h>
#include <core/environment/Singleton.h>
#include <framework/interface/IEngine.h>
#include <render/resources/DescriptorPool.h>

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

        void AddScene(const RDScenePtr &scene);

        void AddViewport(const RDViewportPtr &viewport);

        DescriptorPool* GetGlobalSetPool() const;

        RDTexturePtr GetDefaultTexture() const;

        drv::SamplerPtr GetDefaultSampler() const;

    private:
        friend class Singleton<Render>;

        void InitGlobalPool();

        void InitDefaultResource();

        void InitFonts();

        void InitGui();

        Render() = default;
        ~Render();

        std::vector<RDScenePtr> scenes;
        std::vector<RDViewportPtr> viewports;
        std::unique_ptr<DescriptorPool> globalPool;
        RDTexturePtr defaultTexture;
        drv::SamplerPtr defaultSampler;
    };

}
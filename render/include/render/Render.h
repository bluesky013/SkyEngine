//
// Created by Zach Lee on 2021/11/12.
//

#pragma once
#include <core/environment/Singleton.h>
#include <render/RenderScene.h>
#include <render/RenderView.h>
#include <render/RenderViewport.h>
#include <render/resources/DescriptorPool.h>

namespace sky {

    namespace vk {
        class Instance;
        class Device;
    } // namespace drv

    class RenderScene;

    class Render : public Singleton<Render> {
    public:
        struct RenderInfo {
            std::string appName;
        };

        bool Init(const RenderInfo &);

        void OnTick(float time);

        void AddScene(const RDScenePtr &scene);

        void AddViewport(const RDViewportPtr &viewport);

        DescriptorPool *GetGlobalSetPool() const;

        RDTexturePtr GetDefaultTexture() const;

        vk::SamplerPtr GetDefaultSampler() const;

    private:
        friend class Singleton<Render>;

        void InitGlobalPool();

        void InitDefaultResource();

        void InitFonts();

        void InitGui();

        void InitAssetHandlers();

        Render() = default;
        ~Render();

        std::vector<RDScenePtr>         scenes;
        std::vector<RDViewportPtr>      viewports;
        std::unique_ptr<DescriptorPool> globalPool;
        RDTexturePtr                    defaultTexture;
        vk::SamplerPtr                 defaultSampler;
    };

} // namespace sky

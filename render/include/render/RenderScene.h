
//
// Created by Zach Lee on 2021/11/14.
//


#pragma once

#include <render/RenderView.h>
#include <render/RenderPipeline.h>
#include <render/RenderSceneProxy.h>
#include <render/RenderFeature.h>
#include <core/type/Rtti.h>

namespace sky {

    class RenderScene {
    public:
        RenderScene() = default;
        ~RenderScene() = default;

        void OnPreRender();

        void OnPostRender();

        void OnRender();

        void AddView(RDViewPtr view);

        const std::vector<RDViewPtr>& GetViews() const;

        void Setup(RenderViewport& viewport);

        void ViewportChange(RenderViewport& viewport);

        template <typename T, typename ...Args>
        void RegisterFeature(Args&& ...args)
        {
            features.emplace(TypeInfo<T>::Hash(), std::make_unique<T>(std::forward<Args>(args)...));
        }

        template <typename T>
        T* GetFeature()
        {
            auto iter = features.find(TypeInfo<T>::Hash());
            if (iter == features.end()) {
                return nullptr;
            }
            return static_cast<T*>(iter->second.get());
        }

    private:
        RDPipeline pipeline;
        std::vector<RDViewPtr> views;
        std::unordered_map<uint32_t, std::unique_ptr<RenderFeature>> features;
    };
    using RDScenePtr = std::shared_ptr<RenderScene>;
}
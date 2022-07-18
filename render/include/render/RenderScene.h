
//
// Created by Zach Lee on 2021/11/14.
//


#pragma once

#include <render/RenderView.h>
#include <render/RenderPipeline.h>
#include <render/RenderSceneProxy.h>
#include <render/RenderFeature.h>

namespace sky {

    class RenderScene {
    public:
        RenderScene() = default;
        ~RenderScene() = default;

        void OnPreRender();

        void OnPostRender();

        void OnRender();

        void AddView(RDViewPtr view);

        void RegisterFeature(RenderFeature* feature);

        const std::vector<RDViewPtr>& GetViews() const;

        void Setup(RenderViewport& viewport);

        void ViewportChange(RenderViewport& viewport);

    private:
        RDPipeline pipeline;
        std::vector<RDViewPtr> views;
        std::vector<std::unique_ptr<RenderFeature>> features;
    };
    using RDScenePtr = std::shared_ptr<RenderScene>;
}
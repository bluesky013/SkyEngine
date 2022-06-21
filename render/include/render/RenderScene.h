
//
// Created by Zach Lee on 2021/11/14.
//


#pragma once

#include <render/RenderView.h>
#include <render/RenderPipeline.h>
#include <render/RenderSceneProxy.h>

namespace sky {

    class RenderScene {
    public:
        RenderScene() = default;
        ~RenderScene() = default;

        void OnPreRender();

        void OnPostRender();

        void OnRender();

        void AddView(RDViewPtr view);

        void RemoveView(RDViewPtr view);

        const std::vector<RDViewPtr>& GetViews() const;

    private:
        RDPipeline pipeline;
        std::vector<RDViewPtr> views;
    };
    using RDScenePtr = std::shared_ptr<RenderScene>;
}
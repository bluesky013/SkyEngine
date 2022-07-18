//
// Created by Zach Lee on 2022/7/18.
//

#pragma once

#include <render/RenderFeature.h>
#include <render/RenderView.h>

namespace sky {

    class RenderViewFeature : public RenderFeature {
    public:
        RenderViewFeature() = default;
        ~RenderViewFeature() = default;

        RDViewPtr CreateView();

        void RemoveView(RDViewPtr view);

        void OnPrepareView(RenderScene& scene) override;

        void OnRender(RenderScene& scene) override;

        void OnPostRender(RenderScene& scene) override;

    private:
        std::list<RDViewPtr> views;
    };

}
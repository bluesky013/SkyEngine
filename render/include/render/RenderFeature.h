//
// Created by Zach Lee on 2022/6/24.
//

#pragma once

#include <render/RenderDrawItem.h>
#include <memory>

namespace sky {
    class RenderScene;

    class RenderFeature {
    public:
        RenderFeature() = default;
        virtual ~RenderFeature() = default;

        virtual void OnPrepareView(RenderScene& scene) = 0;

        virtual void GatherRenderItem(RenderScene& scene) = 0;

        virtual void OnRender(RenderScene& scene) = 0;

        virtual void OnPostRender(RenderScene& scene) = 0;
    };

}
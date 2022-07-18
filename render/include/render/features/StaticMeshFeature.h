//
// Created by Zach Lee on 2022/7/18.
//

#pragma once

#include <render/RenderFeature.h>

namespace sky {

    class StaticMeshFeature : public RenderFeature {
    public:
        StaticMeshFeature() = default;
        ~StaticMeshFeature() = default;

        void OnPrepareView(RenderScene& scene) override;

        void GatherRenderItem(RenderScene& scene) override;

        void OnRender(RenderScene& scene) override;

        void OnPostRender(RenderScene& scene) override;
    };

}
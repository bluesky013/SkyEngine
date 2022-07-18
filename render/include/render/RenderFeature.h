//
// Created by Zach Lee on 2022/6/24.
//

#pragma once

#include <render/RenderMeshBatch.h>
#include <memory>

namespace sky {
    class RenderScene;

    class RenderFeature {
    public:
        RenderFeature() = default;
        virtual ~RenderFeature() = default;

        virtual void OnPrepareView(RenderScene& scene) {}

        virtual void GatherRenderItem(RenderScene& scene) {}

        virtual void OnRender(RenderScene& scene) {}

        virtual void OnPostRender(RenderScene& scene) {}
    };

}
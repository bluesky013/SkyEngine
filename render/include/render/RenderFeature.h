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
        RenderFeature(RenderScene& scn) : scene(scn) {}
        virtual ~RenderFeature() = default;

        virtual void OnPreparePipeline() {}

        virtual void GatherRenderPrimitives() {}

        virtual void OnRender() {}

        virtual void OnPostRender() {}

    protected:
        RenderScene& scene;
    };

}
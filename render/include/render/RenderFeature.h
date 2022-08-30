//
// Created by Zach Lee on 2022/6/24.
//

#pragma once

#include <memory>
#include <render/RenderMeshBatch.h>

namespace sky {
    class RenderScene;
    class RenderViewport;

    class RenderFeature {
    public:
        RenderFeature(RenderScene &scn) : scene(scn)
        {
        }
        virtual ~RenderFeature() = default;

        virtual void OnBindViewport(const RenderViewport &viewport)
        {
        }

        virtual void OnViewportSizeChange(const RenderViewport &viewport)
        {
        }

        virtual void OnTick(float time)
        {
        }

        virtual void OnPreparePipeline()
        {
        }

        virtual void GatherRenderPrimitives()
        {
        }

        virtual void OnRender()
        {
        }

        virtual void OnPostRender()
        {
        }

    protected:
        RenderScene &scene;
    };

} // namespace sky
//
// Created by Zach Lee on 2023/4/2.
//

#pragma once

#include <vector>
#include <core/std/Container.h>
#include <render/SceneView.h>
#include <render/RenderPrimitive.h>
#include <render/RenderPipeline.h>
#include <render/FeatureProcessor.h>

namespace sky {

    class RenderScene {
    public:
        void PreTick(float time);
        void PostTick(float time);
        void Render(rdg::RenderGraph& rdg);

        SceneView * CreateSceneView(uint32_t viewCount);
        void RemoveSceneView(SceneView *view);

        void AddPrimitive(RenderPrimitive *primitive);
        void RemovePrimitive(RenderPrimitive *primitive);

        const PmrVector<RenderPrimitive *> &GetPrimitives() const { return primitives; }
        const PmrVector<std::unique_ptr<SceneView>> &GetSceneViews() const { return sceneViews; }

        void AddFeature(IFeatureProcessor *feature);

        template <typename T>
        T *GetFeature() const
        {
            auto iter = features.find(RuntimeTypeId<T>());
            if (iter != features.end()) {
                return static_cast<T*>(iter->second.get());
            }
            return nullptr;
        }
    private:
        friend class Renderer;
        RenderScene();
        ~RenderScene();

        PmrUnSyncPoolRes resources;

        uint32_t viewCounter = 0;

        PmrHashMap<uint32_t, std::unique_ptr<IFeatureProcessor>> features;
        PmrVector<std::unique_ptr<SceneView>> sceneViews;
        PmrVector<RenderPrimitive *> primitives;
    };

} // namespace sky

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
#include <render/RenderBase.h>

namespace sky {

    struct RenderSceneCullingViewData : RefObject {
    };

    class IRenderSceneCulling {
    public:
        IRenderSceneCulling() = default;
        virtual ~IRenderSceneCulling() = default;

        virtual bool IsActive() const noexcept = 0;

        virtual void UpdateByMainView(const Vector3& pos) noexcept {}

        virtual RenderSceneCullingViewData* PrepareCullingViewData(const SceneView* view) const noexcept = 0;

        virtual bool QueryVisible(const RenderSceneCullingViewData* data, uint32_t id) const noexcept = 0;
    protected:
        friend class RenderScene;
        RenderScene* renderScene = nullptr;
    };

    class RenderScene {
    public:
        void SetPersistentID(const Uuid &id) { persistentID = id; }
        const Uuid &GetPersistentID() const { return persistentID; }

        void PreTick(float time);
        void PostTick(float time);
        void Render(rdg::RenderGraph& rdg);

        SceneView *CreateSceneView(uint32_t viewCount);
        void RemoveSceneView(SceneView* view);

        void AttachSceneView(SceneView* sceneView, const Name &name);
        void DetachSceneView(SceneView* sceneView, const Name &name);
        SceneView *GetSceneView(const Name& name) const;
        void SetMainView(const Name& name);

        const PmrHashMap<Name, SceneView*> &GetActiveSceneViews() const { return viewMap; }

        void AddPrimitive(RenderPrimitive *primitive);
        void RemovePrimitive(RenderPrimitive *primitive);

        const PmrVector<RenderPrimitive *> &GetPrimitives() const { return primitives; }
        void AddFeature(IFeatureProcessor *feature);

        void RegisterCullingSystem(const Name& name, IRenderSceneCulling* sys);
        IRenderSceneCulling* GetCullingSystem(const Name& name) const;
        const PmrHashMap<Name, std::unique_ptr<IRenderSceneCulling>> &GetCullingSystem() const { return cullingSystems; }

        const RenderPipelineFlags &GetRenderPipelineFlags() const { return renderFlags; }

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

        Uuid persistentID;

        PmrUnSyncPoolRes resources;

        PmrHashMap<uint32_t, std::unique_ptr<IFeatureProcessor>> features;
        PmrVector<std::unique_ptr<SceneView>> sceneViews;

        Name mainViewName;
        PmrHashMap<Name, SceneView*> viewMap;
        PmrVector<RenderPrimitive *> primitives;

        PmrHashMap<Name, std::unique_ptr<IRenderSceneCulling>> cullingSystems;

        RenderPipelineFlags renderFlags;
    };

} // namespace sky

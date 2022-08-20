
//
// Created by Zach Lee on 2021/11/14.
//


#pragma once

#include <render/RenderView.h>
#include <render/RenderPipeline.h>
#include <render/RenderFeature.h>
#include <render/RenderMesh.h>
#include <render/RenderBufferPool.h>
#include <render/resources/DescirptorGroup.h>
#include <render/resources/DescriptorPool.h>
#include <render/framegraph/FrameGraphEncoder.h>
#include <framework/window/IWindowEvent.h>
#include <core/type/Rtti.h>

namespace sky {

    struct SceneInfo {
        uint32_t lightCount;
    };

    class RenderScene {
    public:
        RenderScene() = default;
        ~RenderScene() = default;

        void ViewportChange(RenderViewport& viewport);

        void BindViewport(RenderViewport& viewport);

        void UpdateOutput(const drv::ImagePtr& output);

        void OnPreRender(float time);

        void OnRender(const drv::CommandBufferPtr& commandBuffer);

        void Setup();

        template <typename T, typename ...Args>
        T* RegisterFeature(Args&& ...args)
        {
            auto iter = features.emplace(TypeInfo<T>::Hash(), std::make_unique<T>(std::forward<Args>(args)...));
            return static_cast<T*>(iter.first->second.get());
        }

        template <typename T>
        T* GetFeature()
        {
            auto iter = features.find(TypeInfo<T>::Hash());
            if (iter == features.end()) {
                return nullptr;
            }
            return static_cast<T*>(iter->second.get());
        }

        const RDDynBufferViewPtr &GetSceneBuffer() const;

        const RDDynBufferViewPtr &GetMainViewBuffer() const;

        DescriptorPool* GetObjectSetPool() const;

        RenderBufferPool* GetObjectBufferPool() const;

        const RDDesGroupPtr &GetSceneSet() const;

        void AddView(const RDViewPtr& view);

        const std::vector<RDViewPtr>& GetViews() const;

        template <typename T, typename ...Args>
        T* RegisterEncoder(uint32_t tag, Args&& ...args)
        {
            auto iter = encoders.find(tag);
            if (iter != encoders.end()) {
                return static_cast<T*>(iter->second.get());
            }
            auto res = static_cast<T*>(encoders.emplace(tag, std::make_unique<T>(std::forward<Args>(args)...)).first->second.get());
            res->SetDrawTag(tag);
            return res;
        }

        void FillSetBinder(drv::DescriptorSetBinder& binder);

    private:
        void InitSceneResource();

        RDPipeline pipeline;
        std::unique_ptr<DescriptorPool> objectPool;
        std::unique_ptr<RenderBufferPool> objectBufferPool;
        std::unordered_map<uint32_t, std::unique_ptr<RenderFeature>> features;
        std::unordered_map<uint32_t, std::unique_ptr<FrameGraphRasterEncoder>> encoders;

        // dynamic data
        std::vector<RDViewPtr> views;
        RDDesGroupPtr sceneSet;
        RDDynBufferViewPtr sceneInfo;
        RDDynBufferViewPtr mainViewInfo;
    };
    using RDScenePtr = std::shared_ptr<RenderScene>;
}
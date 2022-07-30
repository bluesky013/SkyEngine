
//
// Created by Zach Lee on 2021/11/12.
//

#include <render/Render.h>
#include <render/DriverManager.h>
#include <render/DevObjManager.h>
#include <render/RenderConstants.h>
#include <core/logger/Logger.h>
#include <render/features/StaticMeshFeature.h>
#include <render/features/CameraFeature.h>
#include <render/shapes/ShapeManager.h>

static const char* TAG = "Render";

namespace sky {

    Render::~Render()
    {
        ShapeManager::Get()->Destroy();
        DevObjManager::Get()->Destroy();
        DriverManager::Get()->Destroy();
    }

    bool Render::Init(const StartInfo& info)
    {
        LOG_I(TAG, "Init Render");
        if (!DriverManager::Get()->Initialize({info.appName})) {
            LOG_E(TAG, "Init Driver Failed");
            return false;
        }

        InitGlobalPool();
        return true;
    }

    void Render::OnTick(float time)
    {
        for (auto& scene : scenes) {
            scene->OnPreRender();
        }

        for (auto& scene : scenes) {
            scene->OnRender();
        }

        for (auto& scene : scenes) {
            scene->OnPostRender();
        }
    }

    void Render::AddScene(RDScenePtr scene)
    {
        scene->RegisterFeature<CameraFeature>();
        scene->RegisterFeature<StaticMeshFeature>();
        scenes.emplace_back(scene);
    }

    RDDescriptorPoolPtr Render::GetGlobalSetPool() const
    {
        return globalPool;
    }

    void Render::InitGlobalPool()
    {
        drv::DescriptorSetLayout::Descriptor layoutDesc = {};
        layoutDesc.bindings.emplace(0, drv::DescriptorSetLayout::SetBinding{
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT
        });
        layoutDesc.bindings.emplace(1, drv::DescriptorSetLayout::SetBinding{
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT
        });
        auto layout = DriverManager::Get()->GetDevice()->CreateDeviceObject<drv::DescriptorSetLayout>(layoutDesc);
        globalPool = DescriptorPool::CreatePool(layout, {MAX_RENDER_SCENE});
    }
}
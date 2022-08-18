
//
// Created by Zach Lee on 2021/11/12.
//

#include <render/Render.h>
#include <render/DriverManager.h>
#include <render/DevObjManager.h>
#include <render/RenderConstants.h>
#include <render/GlobalDescriptorPool.h>
#include <core/logger/Logger.h>
#include <render/features/StaticMeshFeature.h>
#include <render/features/CameraFeature.h>
#include <render/shapes/ShapeManager.h>

static const char* TAG = "Render";

namespace sky {

    Render::~Render()
    {
        defaultSampler = nullptr;
        defaultTexture = nullptr;
        globalPool = nullptr;

        scenes.clear();
        GlobalDescriptorPool::Get()->Destroy();
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
        InitDefaultResource();
        return true;
    }

    void Render::OnTick(float time)
    {
        for (auto& scene : scenes) {
            scene->OnPreRender(time);
        }

        for (auto& scene : scenes) {
            scene->OnRender();
        }

        for (auto& scene : scenes) {
            scene->OnPostRender();
        }

        DevObjManager::Get()->TickFreeList();
    }

    void Render::AddScene(RDScenePtr scene)
    {
        scene->RegisterFeature<CameraFeature>(*scene);
        scene->RegisterFeature<StaticMeshFeature>(*scene);
        scenes.emplace_back(scene);
    }

    RDDescriptorPoolPtr Render::GetGlobalSetPool() const
    {
        return globalPool;
    }

    RDTexturePtr Render::GetDefaultTexture() const
    {
        return defaultTexture;
    }

    drv::SamplerPtr Render::GetDefaultSampler() const
    {
        return defaultSampler;
    }

    void Render::InitGlobalPool()
    {
        drv::DescriptorSetLayout::Descriptor layoutDesc = {};
        layoutDesc.bindings.emplace(0, drv::DescriptorSetLayout::SetBinding{
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT
        });
        layoutDesc.bindings.emplace(1, drv::DescriptorSetLayout::SetBinding{
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_FRAGMENT_BIT
        });
        auto layout = DriverManager::Get()->GetDevice()->CreateDeviceObject<drv::DescriptorSetLayout>(layoutDesc);
        globalPool = DescriptorPool::CreatePool(layout, {MAX_RENDER_SCENE});
    }

    void Render::InitDefaultResource()
    {
        drv::Sampler::Descriptor samplerDesc = {};
        defaultSampler = DriverManager::Get()->GetDevice()->CreateDeviceObject<drv::Sampler>(samplerDesc);

        uint8_t data[] = {
            127, 127, 127, 255,
        };

        Image::Descriptor imageDesc = {};
        imageDesc.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageDesc.extent = {1, 1};
        imageDesc.mipLevels = 1;
        imageDesc.layers = 1;
        auto image = std::make_shared<Image>(imageDesc);
        image->InitRHI();
        image->Update(data, sizeof(data));

        Texture::Descriptor textureDesc = {};
        defaultTexture = Texture::CreateFromImage(image, textureDesc);
    }
}
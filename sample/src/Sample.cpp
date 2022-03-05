//
// Created by Zach Lee on 2021/11/26.
//

#include <Sample.h>
#include <engine/world/GameObject.h>
#include <engine/render/camera/CameraComponent.h>
#include <engine/render/light/LightComponent.h>
#include <engine/render/model/MeshComponent.h>
#include <framework/asset/AssetManager.h>

namespace sky {

    void Sample::Start()
    {
        NativeWindow::Descriptor des = {
            1366,
            768,
            "SkyEngine",
            "Sample"
        };
        nativeWindow = application.CreateNativeWindow(des);

        viewport = new Viewport(nativeWindow->GetNativeHandle());

        world = new World();

        engine.AddViewport(*viewport);
        engine.AddWorld(*world);

        auto camera = world->CreateGameObject("Camera");
        auto cameraComp = camera->AddComponent<CameraComponent>();
        cameraComp->Perspective(0.1f, 100.f, 60 / 180.f * 3.14f, 1366 / 768.f);

        auto light = world->CreateGameObject("Light");
        light->AddComponent<LightComponent>();

        auto mesh = world->CreateGameObject("Mesh");
        auto meshComp = mesh->AddComponent<MeshComponent>();
        meshComp->asset = Cast<MeshAsset>(
            AssetManager::Get()->FindOrCreate("models/DamagedHelmet.model", MeshAsset::TYPE));

        engine.SetTarget(*world, *viewport);
    }

    void Sample::Stop()
    {
        engine.RemoveWorld(*world);
        delete world;
        world = nullptr;

        engine.RemoveViewport(*viewport);
        delete viewport;
        viewport = nullptr;

        delete viewport;
        delete nativeWindow;
    }

    void Sample::Tick(float delta)
    {

    }

}
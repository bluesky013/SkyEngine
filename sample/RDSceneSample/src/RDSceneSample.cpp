//
// Created by Zach Lee on 2022/7/27.
//

#include <RDSceneSample.h>

#include <core/math/MathUtil.h>

#include <framework/window/NativeWindow.h>

#include <render/Render.h>
#include <render/features/StaticMeshFeature.h>
#include <render/features/CameraFeature.h>

#include <render/RenderCamera.h>

#include <render/resources/Technique.h>
#include <render/resources/Material.h>

#include <render/shapes/ShapeManager.h>
#include <render/shapes/RenderShape.h>
#include <render/RenderPipelineForward.h>
#include <render/RenderConstants.h>

namespace sky {

    void RDSceneSample::Init()
    {
        StartInfo info = {};
        info.appName = "RDSceneSample";

        Render::Get()->Init(info);
    }

    void RDSceneSample::Start()
    {
        scene = std::make_shared<RenderScene>();
        Render::Get()->AddScene(scene);

        viewport = std::make_unique<RenderViewport>();
        auto nativeWindow = Interface<ISystemNotify>::Get()->GetApi()->GetViewport();

        RenderViewport::ViewportInfo info = {};
        info.wHandle = nativeWindow->GetNativeHandle();
        viewport->Setup(info);
        viewport->SetScene(scene);
        auto swapChain = viewport->GetSwapChain();
        auto& ext = swapChain->GetExtent();

        auto cmFeature = scene->GetFeature<CameraFeature>();
        auto smFeature = scene->GetFeature<StaticMeshFeature>();

        mainCamera = cmFeature->Create();
        mainCamera->SetProjectMatrix(glm::perspective(
            60 / 180.f * 3.14f,
            ext.width / static_cast<float>(ext.height),
            0.01f,
            100.f)
        );

        mainCamera->SetTransform(glm::translate(glm::identity<Matrix4>(), Vector3(0, 0, 5)));

        staticMesh = smFeature->Create();
        Matrix4 transform = glm::identity<Matrix4>();
        transform = glm::translate(transform, Vector3(0.0f, -0.5f, 0.5f));
        transform = glm::rotate(transform, glm::radians(30.f), Vector3(1.f, 1.f, 1.f));

        MathUtil::PrintMatrix(transform);
        staticMesh->SetWorldMatrix(transform);

        // init material
        auto colorTable = std::make_shared<GraphicsShaderTable>();
        colorTable->LoadShader("shaders/Standard.vert.spv", "shaders/BaseColor.frag.spv");
        colorTable->InitRHI();

        auto pass = std::make_shared<Pass>();
        SubPassInfo subPassInfo = {};
        subPassInfo.colors.emplace_back(AttachmentInfo{swapChain->GetFormat(), VK_SAMPLE_COUNT_1_BIT});
        subPassInfo.depthStencil = AttachmentInfo{VK_FORMAT_D32_SFLOAT_S8_UINT, VK_SAMPLE_COUNT_1_BIT};
        pass->AddSubPass(subPassInfo);
        pass->InitRHI();

        auto colorTech = std::make_shared<GraphicsTechnique>();
        colorTech->SetShaderTable(colorTable);
        colorTech->SetRenderPass(pass);
        colorTech->SetViewTag(MAIN_CAMERA_TAG);
        colorTech->SetDrawTag(RenderPipelineForward::FORWARD_TAG);

        auto material = std::make_shared<Material>();
        material->AddGfxTechnique(colorTech);

        auto plane= ShapeManager::Get()->GetOrCreate<Plane>();
        auto mesh = plane->CreateMesh(material);
        staticMesh->SetMesh(mesh);
    }

    void RDSceneSample::Stop()
    {
        scene = nullptr;
        Render::Get()->Destroy();
    }

    void RDSceneSample::Tick(float delta)
    {
        Render::Get()->OnTick(delta);
    }

}

REGISTER_MODULE(sky::RDSceneSample)
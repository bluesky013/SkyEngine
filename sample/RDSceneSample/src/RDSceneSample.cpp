//
// Created by Zach Lee on 2022/7/27.
//

#include <RDSceneSample.h>

#include <core/math/MathUtil.h>

#include <framework/window/NativeWindow.h>

#include <render/Render.h>
#include <render/features/CameraFeature.h>
#include <render/features/StaticMeshFeature.h>

#include <render/RenderCamera.h>

#include <render/resources/Material.h>
#include <render/resources/Technique.h>

#include <render/RenderConstants.h>
#include <render/RenderPipelineForward.h>
#include <render/shapes/RenderShape.h>
#include <render/shapes/ShapeManager.h>

namespace sky {

    class RotationFeature : public RenderFeature {
    public:
        RotationFeature(RenderScene &scn) : RenderFeature(scn)
        {
        }
        ~RotationFeature() = default;

        void SetMesh(StaticMesh *value)
        {
            mesh = value;
        }

        void OnTick(float time) override
        {
            Matrix4 transform = glm::identity<Matrix4>();
            transform         = glm::translate(transform, Vector3(0.0f, -0.5f, 0.5f));
            transform         = glm::rotate(transform, glm::radians(angle), Vector3(1.f, 1.f, 1.f));
            mesh->SetWorldMatrix(transform);
            angle += 20.f * time;
        }

    private:
        float       angle = 0.f;
        StaticMesh *mesh  = nullptr;
    };

    void RDSceneSample::Init()
    {
        StartInfo info = {};
        info.appName   = "RDSceneSample";

        Render::Get()->Init(info);
    }

    void RDSceneSample::Start()
    {
        scene = std::make_shared<RenderScene>();
        scene->Setup();
        Render::Get()->AddScene(scene);

        auto viewport     = std::make_shared<RenderViewport>();
        auto nativeWindow = Interface<ISystemNotify>::Get()->GetApi()->GetViewport();

        RenderViewport::ViewportInfo info = {};
        info.wHandle                      = nativeWindow->GetNativeHandle();
        viewport->Setup(info);
        viewport->SetScene(scene);
        Render::Get()->AddViewport(viewport);

        auto  swapChain = viewport->GetSwapChain();
        auto &ext       = swapChain->GetExtent();

        auto cmFeature = scene->GetFeature<CameraFeature>();
        auto smFeature = scene->GetFeature<StaticMeshFeature>();

        mainCamera = cmFeature->Create();
        mainCamera->SetAspect(static_cast<float>(ext.width) / static_cast<float>(ext.height));

        mainCamera->SetTransform(glm::translate(glm::identity<Matrix4>(), Vector3(0, 0, 5)));

        // init material
        auto colorTable = std::make_shared<GraphicsShaderTable>();
        colorTable->LoadShader("shaders/Standard.vert.spv", "shaders/BaseColor.frag.spv");
        colorTable->InitRHI();

        auto        pass        = std::make_shared<Pass>();
        SubPassInfo subPassInfo = {};
        subPassInfo.colors.emplace_back(AttachmentInfo{swapChain->GetFormat(), VK_SAMPLE_COUNT_4_BIT});
        subPassInfo.depthStencil = AttachmentInfo{VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_4_BIT};
        pass->AddSubPass(subPassInfo);
        pass->InitRHI();

        auto colorTech = std::make_shared<GraphicsTechnique>();
        colorTech->SetShaderTable(colorTable);
        colorTech->SetRenderPass(pass);
        colorTech->SetViewTag(MAIN_CAMERA_TAG);
        colorTech->SetDrawTag(FORWARD_TAG);
        colorTech->SetDepthTestEn(true);
        colorTech->SetDepthWriteEn(true);

        material = std::make_shared<Material>();
        material->AddGfxTechnique(colorTech);
        material->InitRHI();

        material->UpdateValue("material.baseColor", Vector4{1.f, 1.f, 1.f, 1.f});
        material->Update();

        staticMesh = smFeature->Create();
        auto cube  = ShapeManager::Get()->GetOrCreate<Cube>();
        auto mesh  = cube->CreateMesh(material);
        staticMesh->SetMesh(mesh);
        staticMesh->SetWorldMatrix(glm::identity<Matrix4>());

        auto feature = scene->RegisterFeature<RotationFeature>(*scene);
        feature->SetMesh(staticMesh);
    }

    void RDSceneSample::Stop()
    {
        scene    = nullptr;
        material = nullptr;
        Render::Get()->Destroy();
    }

    void RDSceneSample::Tick(float delta)
    {
        Render::Get()->OnTick(delta);
    }

} // namespace sky

REGISTER_MODULE(sky::RDSceneSample)
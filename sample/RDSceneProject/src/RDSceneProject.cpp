//
// Created by Zach Lee on 2022/8/13.
//

#include <RDSceneProject.h>
#include <render/Render.h>
#include <render/RenderConstants.h>
#include <render/RenderPipelineForward.h>
#include <render/features/CameraFeature.h>
#include <render/features/StaticMeshFeature.h>
#include <render/imgui/GuiRenderer.h>
#include <imgui.h>
#include <framework/asset/AssetManager.h>
#include <framework/window/NativeWindow.h>
#include <RDSceneProject/EngineRoot.h>
#include <filesystem>
#include <sstream>

namespace sky {

    class RotationFeature : public RenderFeature {
    public:
        explicit RotationFeature(RenderScene& scn) : RenderFeature(scn) {}
        ~RotationFeature() override = default;

        void SetMesh(RenderCamera* value)
        {
            camera = value;
        }

        void OnTick(float time) override
        {
//            auto transform = glm::identity<Matrix4>();
//            transform = glm::translate(transform, Vector3(0, 25, 30));
//            transform = glm::rotate(transform, glm::radians(-30.f), Vector3(1, 0, 0));
//            mainCamera->SetTransform(transform);
//            glm::lookAt()

            angle += glm::radians(30.f) * time;
            position.z = radius * cos(angle);
            position.x = radius * sin(angle);

            auto rotation = glm::eulerAngleYXZ(angle,  -30 / 180.f * 3.14f, 0.f);
            auto transform = glm::translate(glm::identity<Matrix4>(), position);
            transform = transform * rotation;
            camera->SetTransform(transform);
        }

    private:
        float radius = 5.f;
        float angle = 0.f;
        Vector3 position = Vector3(0.f, 2.f, 0.f);
        RenderCamera* camera = nullptr;
    };

    static const char* BUFFER_PATH = "data\\models\\DamagedHelmet_buffer.bin";

    void RDSceneProject::Init()
    {
        AssetManager::Get()->RegisterSearchPath(ENGINE_ROOT);
        AssetManager::Get()->RegisterSearchPath(PROJECT_ROOT);

        StartInfo info = {};
        info.appName = "RDSceneSample";

        Render::Get()->Init(info);
    }

    void RDSceneProject::Start()
    {
        scene = std::make_shared<RenderScene>();
        scene->Setup();
        Render::Get()->AddScene(scene);

        auto viewport = std::make_shared<RenderViewport>();
        auto nativeWindow = Interface<ISystemNotify>::Get()->GetApi()->GetViewport();

        RenderViewport::ViewportInfo info = {};
        info.wHandle = nativeWindow->GetNativeHandle();
        viewport->Setup(info);
        viewport->SetScene(scene);
        Render::Get()->AddViewport(viewport);

        auto swapChain = viewport->GetSwapChain();
        auto& ext = swapChain->GetExtent();

        auto cmFeature = scene->GetFeature<CameraFeature>();
        auto smFeature = scene->GetFeature<StaticMeshFeature>();
        auto guiFeature = scene->GetFeature<GuiRenderer>();

        guiFeature->CreateLambda([](ImGuiContext* context) {
            ImGui::SetCurrentContext(context);
            ImGui::Begin("RDSceneProject");
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();
        });

        mainCamera = cmFeature->Create();
        mainCamera->SetAspect(static_cast<float>(ext.width) / static_cast<float>(ext.height));

        auto feature = scene->RegisterFeature<RotationFeature>(*scene);
        feature->SetMesh(mainCamera);


        // init material
        auto colorTable = std::make_shared<GraphicsShaderTable>();
        colorTable->LoadShader("shaders/Standard.vert.spv", "shaders/BaseColor.frag.spv");
        colorTable->InitRHI();

        auto pass = std::make_shared<Pass>();
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

        auto material = std::make_shared<Material>();
        material->AddGfxTechnique(colorTech);
        material->InitRHI();

        material->UpdateValue("material.baseColor", Vector4{1.f, 1.f, 1.f, 1.f});
        material->Update();

        AssetManager::Get()->LoadAsset<Buffer>(BUFFER_PATH);

        std::stringstream ss;
        ss << "data\\models\\DamagedHelmet_mesh0.mesh";
        std::string path = ss.str();
        auto meshAsset = AssetManager::Get()->LoadAsset<Mesh>(path);
        auto mesh = meshAsset->CreateInstance();
        for (uint32_t i = 0; i < mesh->GetSubMeshCount(); ++i) {
            mesh->SetMaterial(material, i);
        }
        auto staticMesh = smFeature->Create();
        staticMesh->SetMesh(mesh);
        auto transform = glm::identity<Matrix4>();
        transform = glm::rotate(transform, glm::radians(90.f), glm::vec3(1.f, 0.f, 0.f));
        staticMesh->SetWorldMatrix(transform);
    }

    void RDSceneProject::Stop()
    {
        scene = nullptr;
        Render::Get()->Destroy();
    }

    void RDSceneProject::Tick(float delta)
    {
        Render::Get()->OnTick(delta);
    }

}

REGISTER_MODULE(sky::RDSceneProject)
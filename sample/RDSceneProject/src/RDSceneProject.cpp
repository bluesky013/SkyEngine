//
// Created by Zach Lee on 2022/8/13.
//

#include <RDSceneProject.h>
#include <render/Render.h>
#include <render/RenderConstants.h>
#include <render/RenderPipelineForward.h>
#include <render/features/CameraFeature.h>
#include <render/features/StaticMeshFeature.h>
#include <framework/asset/AssetManager.h>
#include <framework/window/NativeWindow.h>
#include <RDSceneProject/EngineRoot.h>
#include <filesystem>
#include <sstream>

namespace sky {

    class RotationFeature : public RenderFeature {
    public:
        RotationFeature(RenderScene& scn) : RenderFeature(scn) {}
        ~RotationFeature() = default;

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

            angle += glm::radians(20.f) * time;
            position.z = radius * cos(angle);
            position.x = radius * sin(angle);

            auto rotation = glm::eulerAngleYXZ(angle,  -30 / 180.f * 3.14f, 0.f);
            auto transform = glm::translate(glm::identity<Matrix4>(), position);
            transform = transform * rotation;
            camera->SetTransform(transform);
        }

    private:
        float radius = 30.f;
        float angle = 0.f;
        Vector3 position = Vector3(0.f, 25.f, 0.f);
        RenderCamera* camera = nullptr;
    };

    static const char* BUFFER_PATH = "data/models/medi2_buffer.bin";

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
            static_cast<float>(ext.width) / static_cast<float>(ext.height),
            0.01f,
            100.f)
        );

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
        colorTech->SetDrawTag(RenderPipelineForward::FORWARD_TAG);
        colorTech->SetDepthTestEn(true);
        colorTech->SetDepthWriteEn(true);

        auto material = std::make_shared<Material>();
        material->AddGfxTechnique(colorTech);
        material->InitRHI();

        material->UpdateValue("material.baseColor", Vector4{1.f, 1.f, 1.f, 1.f});
        material->Update();

        std::filesystem::path temp(BUFFER_PATH);
        auto str = temp.string();
        AssetManager::Get()->RegisterAsset(Uuid::CreateWithSeed(4059331220), BUFFER_PATH);

        for (uint32_t i = 0; i < 20; ++i) {
            std::stringstream ss;
            ss << "data/models/medi2_mesh" << i << ".mesh";
            std::string path = ss.str();

            AssetManager::Get()->RegisterAsset(Uuid::CreateWithSeed(Fnv1a32(path)), path);
            auto meshAsset = AssetManager::Get()->LoadAsset<Mesh>(Uuid::CreateWithSeed(Fnv1a32(path)));
            auto mesh = meshAsset->CreateInstance();
            for (uint32_t i = 0; i < mesh->GetSubMeshCount(); ++i) {
                mesh->SetMaterial(material, i);
            }
            auto staticMesh = smFeature->Create();
            staticMesh->SetMesh(mesh);
            auto transform = glm::identity<Matrix4>();
            transform = glm::scale(transform, Vector3(0.01f, 0.01f, 0.01f));
            staticMesh->SetWorldMatrix(transform);
        }
    }

    void RDSceneProject::Stop()
    {
        scene = nullptr;
        viewport = nullptr;
        Render::Get()->Destroy();
    }

    void RDSceneProject::Tick(float delta)
    {
        Render::Get()->OnTick(delta);
    }

}

REGISTER_MODULE(sky::RDSceneProject)
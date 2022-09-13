//
// Created by Zach Lee on 2022/8/13.
//

#include <RDSceneProject.h>
#include <RDSceneProject/EngineRoot.h>
#include <filesystem>
#include <framework/asset/AssetManager.h>
#include <framework/window/NativeWindow.h>
#include <imgui.h>
#include <render/Render.h>
#include <render/RenderConstants.h>
#include <render/RenderPipelineForward.h>
#include <render/features/CameraFeature.h>
#include <render/features/StaticMeshFeature.h>
#include <render/imgui/GuiRenderer.h>
#include <render/resources/Prefab.h>

#include <core/math/Quaternion.h>
#include <core/math/Matrix3.h>
#include <core/math/Matrix4.h>
#include <sstream>

namespace sky {

    static const std::map<VkQueryPipelineStatisticFlagBits, std::string> PIPELINE_STATS_MAP = {
        {VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, "input assembly vertices            "},
        {VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, "input assembly primitives          "},
        {VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, "vertex shader invocations          "},
        {VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT, "geometry shader invocations        "},
        {VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT, "geometry shader primitives         "},
        {VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT, "clipping invocations               "},
        {VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT, "clipping primitives                "},
        {VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT, "fragment shader invocations        "},
        {VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT, "tess control shader patches        "},
        {VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT, "tess evaluation shader invocations "},
        {VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT, "compute shader invocations         "},
    };

    class RotationFeature : public RenderFeature {
    public:
        explicit RotationFeature(RenderScene &scn) : RenderFeature(scn)
        {
        }
        ~RotationFeature() override = default;

        void SetCamera(RenderCamera *value)
        {
            camera = value;
        }

        void SetMeshes(std::vector<StaticMesh*>& meshList, std::vector<Transform> &transforms)
        {
            meshes = meshList.data();
            trans = transforms.data();
            meshNum = static_cast<uint32_t>(meshList.size());
        }

        void OnTick(float time) override
        {
            //            auto transform = glm::identity<Matrix4>();
            //            transform = glm::translate(transform, Vector3(0, 25, 30));
            //            transform = glm::rotate(transform, glm::radians(-30.f), Vector3(1, 0, 0));
            //            mainCamera->SetTransform(transform);
            //            glm::lookAt()

            angle += ToRadian(30.f) * time;
            position.z = radius * cos(angle);
            position.x = radius * sin(angle);

            Matrix4 translation = Matrix4::Identity();
            translation.Translate(position);
            auto rotation  = Cast(Matrix3::FromEulerYXZ({ToRadian(-30.f), angle, 0.f}));

//            rotation.Translate(position);
//            auto transform = glm::translate(glm::identity<Matrix4>(), position);
//            transform      = transform * rotation;
            camera->SetTransform(translation * rotation);

//            JobSystem *js = JobSystem::Get();
//            tf::Taskflow flow;
//            uint32_t nPerG = 256;
//            uint32_t group = meshNum / nPerG + 1;
//            for (uint32_t i = 0; i < group; ++i) {
//                flow.emplace([i, nPerG, this]() {
//                    for (uint32_t j = 0, k = i * nPerG; j < nPerG && k < meshNum; ++j, ++k) {
//                        auto tt = glm::identity<Matrix4>();
//                        tt      = glm::translate(tt, trans[k].position + Vector3(0.f, rand() % 100 / 100.f, 0.f));
//                        tt      = glm::rotate(tt, glm::radians(90.f), Vector3(1.f, 0.f, 0.f));
//                        tt      = glm::scale(tt, trans[k].scale);
//                        meshes[k]->SetWorldMatrix(tt);
//                    }
//                });
//            }
//            js->Run(std::move(flow)).wait();

//            for (uint32_t i = 0; i < meshNum; ++i) {
//                auto tt = glm::identity<Matrix4>();
//                tt = glm::translate(tt, trans[i].position + Vector3(0.f, rand() % 100 / 100.f, 0.f));
//                tt = glm::rotate(tt, glm::radians(90.f), Vector3(1.f, 0.f, 0.f));
//                tt = glm::scale(tt, trans[i].scale);
//                meshes[i]->SetWorldMatrix(tt);
//            }
        }

    private:
        float         radius    = 5.f;
        float         angle     = 0.f;
        Vector3       position  = Vector3(0.f, 2.f, 0.f);
        RenderCamera *camera    = nullptr;
        StaticMesh  **meshes    = nullptr;
        Transform    *trans     = nullptr;
        uint32_t      meshNum   = 0;
    };

    static const char *BUFFER_PATH = "data\\models\\DamagedHelmet_buffer0.buffer";

    void RDSceneProject::Init()
    {
        AssetManager::Get()->RegisterSearchPath(ENGINE_ROOT);
        AssetManager::Get()->RegisterSearchPath(PROJECT_ROOT);

        StartInfo info = {};
        info.appName   = "RDSceneSample";

        Render::Get()->Init(info);
    }

    void RDSceneProject::Start()
    {
        scene = std::make_shared<RenderScene>();
        scene->Setup();
        Render::Get()->AddScene(scene);

        auto viewport     = std::make_shared<RenderViewport>();
        const auto *nativeWindow = Interface<ISystemNotify>::Get()->GetApi()->GetViewport();

        RenderViewport::ViewportInfo info = {};
        info.wHandle                      = nativeWindow->GetNativeHandle();
        viewport->Setup(info);
        viewport->SetScene(scene);
        Render::Get()->AddViewport(viewport);

        auto  swapChain = viewport->GetSwapChain();
        const auto &ext       = swapChain->GetExtent();

        auto *cmFeature  = scene->GetFeature<CameraFeature>();
        auto *smFeature  = scene->GetFeature<StaticMeshFeature>();
        auto *guiFeature = scene->GetFeature<GuiRenderer>();

        guiFeature->CreateLambda([this](ImGuiContext *context) {
            ImGui::SetCurrentContext(context);
            ImGui::Begin("RDSceneProject");
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::Separator();

            if (ImGui::CollapsingHeader("Pipeline statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
                const auto                         &pool  = scene->GetQueryPool();
                const auto                         &data  = pool->GetData();
                VkQueryPipelineStatisticFlags flags = pool->GetFlags();
                uint32_t                      index = 0;
                for (const auto &[flag, str] : PIPELINE_STATS_MAP) {
                    if ((flags & flag) == flag) {
                        ImGui::Text("%s : [%llu]", str.c_str(), data[index++]);
                    }
                }
            }

            ImGui::End();
        });

        mainCamera = cmFeature->Create();
        mainCamera->SetAspect(static_cast<float>(ext.width) / static_cast<float>(ext.height));


        auto prefabAsset = AssetManager::Get()->LoadAsset<Prefab>("data\\models\\DamagedHelmet.prefab");
        auto prefab = prefabAsset->CreateInstance();
        prefab->LoadToScene(*scene);

        auto matAsset = AssetManager::Get()->LoadAsset<Material>("data\\models\\DamagedHelmet_mat0.mat");
        matAsset->CreateInstance();

//        uint32_t num = 100;
//        for (uint32_t i = 0; i < num; ++i) {
//            for (uint32_t j = 0; j < num; ++j) {
//                auto *staticMesh = smFeature->Create();
//                staticMesh->SetMesh(mesh);
//                auto transform = glm::identity<Matrix4>();
//                transform      = glm::translate(transform, Vector3(i - num / 2.f, 0.f, j - num / 2.f));
//                transform      = glm::rotate(transform, glm::radians(90.f), Vector3(1.f, 0.f, 0.f));
//                transform      = glm::scale(transform, Vector3(0.2f, 0.2f, 0.2f));
//                staticMesh->SetWorldMatrix(transform);
//                meshes.emplace_back(staticMesh);
//                transforms.emplace_back(Transform{
//                    {i - num / 2.f, 0.f, j - num / 2.f},
//                    {90.f, 0.f, 0.f},
//                    {0.2f, 0.2f, 0.2f}
//                });
//            }
//        }

        auto *feature = scene->RegisterFeature<RotationFeature>(*scene);
        feature->SetCamera(mainCamera);
        feature->SetMeshes(meshes, transforms);
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

} // namespace sky

REGISTER_MODULE(sky::RDSceneProject)
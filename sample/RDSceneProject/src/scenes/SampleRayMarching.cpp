//
// Created by blues on 2023/9/18.
//

#include "SampleRayMarching.h"
#include <framework/asset/AssetManager.h>
#include <framework/world/TransformComponent.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>

#include <render/adaptor/components/CameraComponent.h>
#include <render/adaptor/components/MeshRenderer.h>

#include <render/adaptor/Util.h>
#include <render/RenderWindow.h>
#include <render/rdg/RenderGraph.h>
#include <imgui/ImGuiFeatureProcessor.h>
#include <imgui/ImGuiFeature.h>

namespace sky {
    class ForwardWithRayMarching : public RenderPipeline {
    public:
        ForwardWithRayMarching() = default;
        ~ForwardWithRayMarching() override = default;

        void SetOutput(RenderWindow *wnd)
        {
            output = wnd;
            if (!rdgContext->device->CheckFormatFeature(depthStencilFormat, rhi::PixelFormatFeatureFlagBit::DEPTH_STENCIL)) {
                depthStencilFormat = rhi::PixelFormat::D32_S8;
            }

            globalUbo = std::make_shared<UniformBuffer>();
            globalUbo->Init(sizeof(ShaderPassInfo));

            {
                rhi::DescriptorSetLayout::Descriptor desc = {};
                desc.bindings.emplace_back(rhi::DescriptorSetLayout::SetBinding {rhi::DescriptorType::UNIFORM_BUFFER, 1, 0, rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS, "passInfo"});
                desc.bindings.emplace_back(rhi::DescriptorSetLayout::SetBinding {rhi::DescriptorType::UNIFORM_BUFFER, 1, 1, rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS, "viewInfo"});

                forwardLayout = std::make_shared<ResourceGroupLayout>();
                forwardLayout->SetRHILayout(rdgContext->device->CreateDescriptorSetLayout(desc));
                forwardLayout->AddNameHandler("passInfo", {0, sizeof(ShaderPassInfo)});
                forwardLayout->AddNameHandler("viewInfo", {1, sizeof(SceneViewInfo)});
            }
        }

        void OnSetup(rdg::RenderGraph &rdg) override
        {
            const auto &swapchain = output->GetSwaChain();
            const auto &ext = swapchain->GetExtent();
            const auto width  = ext.width;
            const auto height = ext.height;

            auto &rg = rdg.resourceGraph;

            const auto &views = rdg.scene->GetSceneViews();
            if (views.empty()) {
                return;
            }
            auto *sceneView = views[0].get();
            const auto uboName = GetDefaultSceneViewUBOName(*sceneView);
            const auto *const globalUboName = "globalUBO";

            ShaderPassInfo passInfo = {};
            passInfo.viewport.x = 0;
            passInfo.viewport.y = 0;
            passInfo.viewport.z = static_cast<float>(width);
            passInfo.viewport.w = static_cast<float>(height);
            globalUbo->Write(0, passInfo);

            rg.ImportUBO(uboName.c_str(), sceneView->GetUBO());
            rg.ImportUBO(globalUboName, globalUbo);

            rg.ImportSwapChain("ForwardColor", swapchain);
            rg.AddImage("ForwardDepthStencil", rdg::GraphImage{{width, height, 1}, 1, 1, depthStencilFormat, rhi::ImageUsageFlagBit::DEPTH_STENCIL | rhi::ImageUsageFlagBit::SAMPLED});
            auto forwardPass = rdg.AddRasterPass("forwardColor", width, height)
                                   .AddAttachment({"ForwardColor", rhi::LoadOp::CLEAR, rhi::StoreOp::STORE}, rhi::ClearValue(0.2f, 0.2f, 0.2f, 1.f))
                                   .AddAttachment({"ForwardDepthStencil", rhi::LoadOp::CLEAR, rhi::StoreOp::STORE}, rhi::ClearValue(1.f, 0));

            auto subpass = forwardPass.AddRasterSubPass("color0_sub0");
            subpass.AddColor("ForwardColor", rdg::ResourceAccessBit::WRITE)
                .AddDepthStencil("ForwardDepthStencil", rdg::ResourceAccessBit::WRITE)
                .AddComputeView(uboName, {"viewInfo", rdg::ComputeType::CBV, rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS})
                .AddComputeView(globalUboName, {"passInfo", rdg::ComputeType::CBV, rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS});

            subpass.AddQueue("queue1")
                .SetRasterID("ForwardColor")
                .SetView(sceneView)
                .SetLayout(forwardLayout);

            subpass.AddQueue("queue2")
                .SetRasterID("ForwardVolume")
                .SetView(sceneView)
                .SetLayout(forwardLayout);

            subpass.AddQueue("queue3")
                .SetRasterID("ui");

            rdg.AddPresentPass("present", "ForwardColor");
        }

    private:
        RenderWindow *output = nullptr;
        rhi::PixelFormat depthStencilFormat = rhi::PixelFormat::D24_S8;
        RDResourceLayoutPtr forwardLayout;
        RDUniformBufferPtr globalUbo;
    };

    bool SampleRayMarching::Start(RenderWindow *window)
    {
        if (!SampleScene::Start(window)) {
            return false;
        }
        meshObj = world->CreateGameObject("Cube");
        auto *mesh = meshObj->AddComponent<MeshRenderer>();
        auto volumeMat = AssetManager::Get()->LoadAsset<MaterialInstance>("materials/volume_simple.mati");
        auto meshAsset = AssetManager::Get()->LoadAsset<Mesh>("DamagedHelmet/DamagedHelmet_mesh_0.mesh");
        mesh->SetMesh(meshAsset);

        auto *scene = GetRenderSceneFromGameObject(meshObj);
        auto *gui = GetFeatureProcessor<ImGuiFeatureProcessor>(scene)->CreateGUIInstance();
        gui->BindNativeWindow(Interface<ISystemNotify>::Get()->GetApi()->GetViewport());
        gui->AddFunctions([](ImGuiContext *context) {
            ImGui::SetCurrentContext(context);
            auto &io = ImGui::GetIO();
            ImGui::Begin("Hello World");
            ImGui::Text("Application average %.3f ms / frame (%.1f FPS)", 1000.f / io.Framerate, io.Framerate);
            ImGui::End();
        });

        camera = world->CreateGameObject("MainCamera");
        auto *cc = camera->AddComponent<CameraComponent>();
        cc->Perspective(0.01f, 100.f, 45.f / 180.f * 3.14f);
        cc->SetAspect(window->GetWidth(), window->GetHeight());

        auto *cameraTrans = camera->GetComponent<TransformComponent>();
        cameraTrans->SetWorldTranslation(Vector3(5, 1, 5));
        cameraTrans->SetWorldRotation(Quaternion(45.f / 180.f * 3.14f, VEC3_Y));

        auto *pipeline = new ForwardWithRayMarching();
        pipeline->SetOutput(window);
        scene->SetPipeline(pipeline);
        return true;
    }

    void SampleRayMarching::Shutdown()
    {
        SampleScene::Shutdown();
    }

    void SampleRayMarching::Resize(uint32_t width, uint32_t height)
    {
        camera->GetComponent<CameraComponent>()->SetAspect(width, height);
    }


} // namespace sky
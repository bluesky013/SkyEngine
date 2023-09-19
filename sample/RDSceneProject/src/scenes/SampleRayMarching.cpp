//
// Created by blues on 2023/9/18.
//

#include "SampleRayMarching.h"
#include <framework/asset/AssetManager.h>
#include <render/adaptor/components/CameraComponent.h>
#include <render/adaptor/components/MeshRenderer.h>
#include <framework/world/TransformComponent.h>
#include <render/adaptor/Util.h>
#include <render/RenderWindow.h>
#include <render/rdg/RenderGraph.h>

namespace sky {
    class ForwardWithRayMarching : public RenderPipeline {
    public:
        ForwardWithRayMarching()
        {
            rayMarchingTech = AssetManager::Get()->LoadAsset<Technique>("techniques/ray_marching.tech")->CreateInstanceAs<GraphicsTechnique>();
        }

        ~ForwardWithRayMarching() override = default;

        void SetOutput(RenderWindow *wnd)
        {
            output = wnd;
            if (!rdgContext->device->CheckFormatFeature(depthStencilFormat, rhi::PixelFormatFeatureFlagBit::DEPTH_STENCIL)) {
                depthStencilFormat = rhi::PixelFormat::D32_S8;
            }

            {
                rhi::DescriptorSetLayout::Descriptor desc = {};
                desc.bindings.emplace_back(rhi::DescriptorSetLayout::SetBinding {rhi::DescriptorType::UNIFORM_BUFFER, 1, 0, rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS, "viewInfo"});

                forwardLayout = std::make_shared<ResourceGroupLayout>();
                forwardLayout->SetRHILayout(rdgContext->device->CreateDescriptorSetLayout(desc));
                forwardLayout->AddNameHandler("viewInfo", {0, sizeof(SceneViewInfo)});
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
            rg.ImportUBO(uboName.c_str(), sceneView->GetUBO());

            rg.ImportSwapChain("FinalOut", swapchain);
            rg.AddImage("ForwardColor", rdg::GraphImage{{width, height, 1}, 1, 1, rhi::PixelFormat::RGBA8_UNORM, rhi::ImageUsageFlagBit::RENDER_TARGET | rhi::ImageUsageFlagBit::SAMPLED});
            rg.AddImage("ForwardDepthStencil", rdg::GraphImage{{width, height, 1}, 1, 1, depthStencilFormat, rhi::ImageUsageFlagBit::DEPTH_STENCIL | rhi::ImageUsageFlagBit::SAMPLED});
            rg.AddImageView("ForwardDepthStencil_d", "ForwardDepthStencil", rdg::GraphImageView{0, 1, 0, 1, rhi::AspectFlagBit::DEPTH_BIT});
            auto forwardPass = rdg.AddRasterPass("forwardColor", width, height)
                                   .AddAttachment({"ForwardColor", rhi::LoadOp::CLEAR, rhi::StoreOp::STORE}, rhi::ClearValue(0.2f, 0.2f, 0.2f, 1.f))
                                   .AddAttachment({"ForwardDepthStencil", rhi::LoadOp::CLEAR, rhi::StoreOp::STORE}, rhi::ClearValue(1.f, 0));

            forwardPass.AddRasterSubPass("color0_sub0")
                .AddColor("ForwardColor", rdg::ResourceAccessBit::WRITE)
                .AddDepthStencil("ForwardDepthStencil", rdg::ResourceAccessBit::WRITE)
                .AddComputeView(uboName, {"viewInfo", rdg::ComputeType::CBV, rhi::ShaderStageFlagBit::VS})
                .AddQueue("queue1")
                    .SetRasterID("ForwardColor")
                    .SetView(sceneView)
                    .SetLayout(forwardLayout);

            auto rayMarchingPass = rdg.AddRasterPass("rayMarching", width, height)
                                   .AddAttachment({"FinalOut", rhi::LoadOp::DONT_CARE, rhi::StoreOp::STORE}, rhi::ClearValue{0, 0, 0, 0});

            rayMarchingPass.AddRasterSubPass("rayMarching_sub0")
                .AddColor("FinalOut", rdg::ResourceAccessBit::WRITE)
                .AddComputeView("ForwardColor", {"mainColor", rdg::ComputeType::SRV, rhi::ShaderStageFlagBit::FS})
                .AddComputeView("ForwardDepthStencil_d", {"mainDepth", rdg::ComputeType::SRV, rhi::ShaderStageFlagBit::FS})
                .AddComputeView(uboName, {"viewInfo", rdg::ComputeType::CBV, rhi::ShaderStageFlagBit::FS})
                .AddFullScreen("marching")
                    .SetTechnique(rayMarchingTech);

            rdg.AddPresentPass("present", "FinalOut");
        }

    private:
        RenderWindow *output = nullptr;
        rhi::PixelFormat depthStencilFormat = rhi::PixelFormat::D24_S8;
        RDResourceLayoutPtr forwardLayout;
        RDGfxTechPtr rayMarchingTech;
    };

    bool SampleRayMarching::Start(RenderWindow *window)
    {
        if (!SampleScene::Start(window)) {
            return false;
        }
        meshObj = world->CreateGameObject("Cube");
        auto *mesh = meshObj->AddComponent<MeshRenderer>();
        AssetManager::Get()->LoadAsset<Texture>("images/test.image")->CreateInstance();
        mesh->SetMesh(AssetManager::Get()->LoadAsset<Mesh>("DamagedHelmet/DamagedHelmet_mesh_0.mesh"));

        camera = world->CreateGameObject("MainCamera");
        auto *cc = camera->AddComponent<CameraComponent>();
        cc->Perspective(0.01f, 100.f, 45.f / 180.f * 3.14f);
        cc->SetAspect(window->GetWidth(), window->GetHeight());

        auto *cameraTrans = camera->GetComponent<TransformComponent>();
        cameraTrans->SetWorldTranslation(Vector3(5, 1, 5));
        cameraTrans->SetWorldRotation(Quaternion(45.f / 180.f * 3.14f, VEC3_Y));

        auto *pipeline = new ForwardWithRayMarching();
        pipeline->SetOutput(window);

        auto *scene = GetRenderSceneFromGameObject(meshObj);
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
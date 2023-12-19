//
// Created by Zach Lee on 2023/9/16.
//

#include "SampleMesh.h"
#include <render/RenderWindow.h>
#include <render/adaptor/components/CameraComponent.h>
#include <render/adaptor/components/MeshRenderer.h>
#include <render/adaptor/assets/MeshAsset.h>
#include <render/adaptor/Util.h>
#include <render/geometry/GeometryRenderer.h>

#include <render/rdg/RenderGraph.h>

#include <render/mesh/GridRenderer.h>

#include "SimpleRotateComponent.h"

namespace sky {
    class ForwardMSAAPass : public RenderPipeline {
    public:
        ForwardMSAAPass() = default;
        ~ForwardMSAAPass() override = default;

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

            postTech = AssetManager::Get()->LoadAsset<Technique>("techniques/post_processing.tech")->CreateInstanceAs<GraphicsTechnique>();
        }

        void OnSetup(rdg::RenderGraph &rdg) override
        {
            const auto &swapchain = output->GetSwaChain();
            const auto &ext = swapchain->GetExtent();
            const auto width  = ext.width * 2;
            const auto height = ext.height * 2;

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

            rhi::PixelFormat hdrFormat = rhi::PixelFormat::RGBA16_SFLOAT;

            rg.ImportSwapChain("SwapChain", swapchain);
            rg.AddImage("ForwardColor", rdg::GraphImage{{width, height, 1}, 1, 1, hdrFormat, rhi::ImageUsageFlagBit::RENDER_TARGET | rhi::ImageUsageFlagBit::SAMPLED, rhi::SampleCount::X1});
            rg.AddImage("ForwardColorMSAA", rdg::GraphImage{{width, height, 1}, 1, 1, hdrFormat, rhi::ImageUsageFlagBit::RENDER_TARGET, rhi::SampleCount::X8});
            rg.AddImage("ForwardDSMSAA", rdg::GraphImage{{width, height, 1}, 1, 1, depthStencilFormat, rhi::ImageUsageFlagBit::DEPTH_STENCIL, rhi::SampleCount::X8});
            auto forwardPass = rdg.AddRasterPass("forwardColor", width, height)
                                   .AddAttachment({"ForwardColor", rhi::LoadOp::CLEAR, rhi::StoreOp::STORE}, rhi::ClearValue(0.2f, 0.2f, 0.2f, 1.f))
                                   .AddAttachment({"ForwardColorMSAA", rhi::LoadOp::CLEAR, rhi::StoreOp::STORE}, rhi::ClearValue(0.2f, 0.2f, 0.2f, 1.f))
                                   .AddAttachment({"ForwardDSMSAA", rhi::LoadOp::CLEAR, rhi::StoreOp::STORE}, rhi::ClearValue(1.f, 0));

            auto subpass = forwardPass.AddRasterSubPass("color0_sub0");
            subpass.AddColor("ForwardColorMSAA", rdg::ResourceAccessBit::WRITE)
                .AddDepthStencil("ForwardDSMSAA", rdg::ResourceAccessBit::WRITE)
                .AddResolve("ForwardColor", rdg::ResourceAccessBit::WRITE)
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

            auto pp = rdg.AddRasterPass("PostProcessing", ext.width, ext.height)
                      .AddAttachment({"SwapChain", rhi::LoadOp::DONT_CARE, rhi::StoreOp::STORE}, {});

            auto ppSub = pp.AddRasterSubPass("pp_sub0");

            ppSub.AddColor("SwapChain", rdg::ResourceAccessBit::WRITE)
                .AddComputeView("ForwardColor", {"colorImage", rdg::ComputeType::SRV, rhi::ShaderStageFlagBit::FS});

            ppSub.AddFullScreen("fullscreen")
                .SetTechnique(postTech);

            rdg.AddPresentPass("present", "SwapChain");
        }

    private:
        RenderWindow *output = nullptr;
        rhi::PixelFormat depthStencilFormat = rhi::PixelFormat::D24_S8;
        RDResourceLayoutPtr forwardLayout;
        RDGfxTechPtr postTech;
        RDUniformBufferPtr globalUbo;
    };

    bool SampleMesh::Start(sky::RenderWindow *window)
    {
        if (!SampleScene::Start(window)) {
            return false;
        }

        meshObj = world->CreateGameObject("Cube");
        meshObj->GetComponent<TransformComponent>()->SetLocalRotation(
            Quaternion(-30 / 180.f * 3.14f, VEC3_Y) *
            Quaternion(90 / 180.f * 3.14f, VEC3_X));
        meshObj->AddComponent<SimpleRotateComponent>();

        auto *mesh = meshObj->AddComponent<MeshRenderer>();
        mesh->SetMesh(AssetManager::Get()->LoadAsset<Mesh>("DamagedHelmet/DamagedHelmet_mesh_0.mesh"));

//        auto mat = AssetManager::Get()->LoadAsset<MaterialInstance>("materials/floor.mati")->CreateInstance();
//        auto floor = GridRenderer().SetUp({512}).BuildMesh(mat);
//        mesh->SetMesh(floor);

        camera = world->CreateGameObject("MainCamera");
        auto *cc = camera->AddComponent<CameraComponent>();
        cc->Perspective(0.01f, 100.f, 45.f / 180.f * 3.14f);
        cc->SetAspect(window->GetWidth(), window->GetHeight());
        camera->GetComponent<TransformComponent>()->SetWorldTranslation(Vector3(0, 0.5, 5));

        auto *scene = GetRenderSceneFromGameObject(meshObj);
        auto *pipeline = new ForwardMSAAPass();
        pipeline->SetOutput(window);
        scene->SetPipeline(pipeline);
        return true;
    }

    void SampleMesh::Shutdown()
    {
        SampleScene::Shutdown();
    }

    void SampleMesh::Resize(uint32_t width, uint32_t height)
    {
        camera->GetComponent<CameraComponent>()->SetAspect(width, height);
    }
} // namespace sky
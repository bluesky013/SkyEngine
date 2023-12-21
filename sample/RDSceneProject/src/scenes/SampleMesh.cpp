//
// Created by Zach Lee on 2023/9/16.
//

#include "SampleMesh.h"
#include <render/RenderWindow.h>
#include <render/adaptor/components/CameraComponent.h>
#include <render/adaptor/components/MeshRenderer.h>
#include <render/adaptor/assets/MeshAsset.h>
#include <render/adaptor/Util.h>
#include <core/file/FileIO.h>
#include <rhi/Decode.h>
#include <render/RHI.h>

#include <render/rdg/RenderGraph.h>
#include <render/env/SkyBoxRenderer.h>
#include <render/mesh/GridRenderer.h>

#include "SimpleRotateComponent.h"

#include <RDSceneProject/ProjectRoot.h>

namespace sky {
    RDTextureCubePtr LoadCubeMap(const std::string &path)
    {
        std::vector<uint8_t> data;
        ReadBin(PROJECT_ROOT + path, data);

        rhi::Image::Descriptor imageDesc = {};
        imageDesc.arrayLayers = 6;

        std::vector<rhi::ImageUploadRequest> requests;
        rhi::ProcessDDS(data.data(), data.size(), imageDesc, requests);

        auto tex = std::make_shared<TextureCube>();
        auto *queue = RHI::Get()->GetDevice()->GetQueue(rhi::QueueType::TRANSFER);
        tex->Init(imageDesc.format, imageDesc.extent.width, imageDesc.extent.height, imageDesc.mipLevels);
        auto handle = queue->UploadImage(tex->GetImage(), requests);
        queue->Wait(handle);

        return tex;
    }

    class ForwardMSAAPass : public RenderPipeline {
    public:
        ForwardMSAAPass() = default;
        ~ForwardMSAAPass() override = default;

        void SetOutput(RenderWindow *wnd, RenderScene *scene)
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
                desc.bindings.emplace_back(rhi::DescriptorSetLayout::SetBinding {rhi::DescriptorType::COMBINED_IMAGE_SAMPLER, 1, 2, rhi::ShaderStageFlagBit::FS, "irradianceMap"});
                desc.bindings.emplace_back(rhi::DescriptorSetLayout::SetBinding {rhi::DescriptorType::COMBINED_IMAGE_SAMPLER, 1, 3, rhi::ShaderStageFlagBit::FS, "radianceMap"});
                desc.bindings.emplace_back(rhi::DescriptorSetLayout::SetBinding {rhi::DescriptorType::COMBINED_IMAGE_SAMPLER, 1, 4, rhi::ShaderStageFlagBit::FS, "brdfLutMap"});

                forwardLayout = std::make_shared<ResourceGroupLayout>();
                forwardLayout->SetRHILayout(rdgContext->device->CreateDescriptorSetLayout(desc));
                forwardLayout->AddNameHandler("passInfo", {0, sizeof(ShaderPassInfo)});
                forwardLayout->AddNameHandler("viewInfo", {1, sizeof(SceneViewInfo)});
                forwardLayout->AddNameHandler("irradianceMap", {2, 0});
                forwardLayout->AddNameHandler("radianceMap", {3, 0});
                forwardLayout->AddNameHandler("brdfLutMap", {4, 0});
            }

            postTech = AssetManager::Get()->LoadAsset<Technique>("techniques/post_processing.tech")->CreateInstanceAs<GraphicsTechnique>();
            brdfLutTech = AssetManager::Get()->LoadAsset<Technique>("techniques/brdf_lut.tech")->CreateInstanceAs<GraphicsTechnique>();
            auto skyboxTex = LoadCubeMap("/assets/skybox/output_skybox.dds");
            irradiance = LoadCubeMap("/assets/skybox/output_iem.dds");
            radiance = LoadCubeMap("/assets/skybox/output_pmrem.dds");

            auto skyboxMat = AssetManager::Get()->LoadAsset<Material>("materials/skybox.mat")->CreateInstance();
            auto skyboxMatInst = std::make_shared<MaterialInstance>();
            skyboxMatInst->SetMaterial(skyboxMat);
            skyboxMatInst->SetTexture("skybox", skyboxTex, 0);
            skyboxMatInst->Upload();

            skybox = std::make_unique<SkyBoxRenderer>();
            skybox->SetUp(skyboxMatInst);
            skybox->AttachScene(scene);
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
            const uint32_t BRDFLutSize = 512;

            rg.ImportSwapChain("SwapChain", swapchain);
            rg.ImportImage("Radiance", radiance->GetImage(), radiance->GetImageView()->GetViewDesc().viewType);
            rg.ImportImage("Irradiance", irradiance->GetImage(), radiance->GetImageView()->GetViewDesc().viewType);
            rg.AddImage("ForwardColor", rdg::GraphImage{{width, height, 1}, 1, 1, hdrFormat, rhi::ImageUsageFlagBit::RENDER_TARGET | rhi::ImageUsageFlagBit::SAMPLED, rhi::SampleCount::X1});
            rg.AddImage("ForwardColorMSAA", rdg::GraphImage{{width, height, 1}, 1, 1, hdrFormat, rhi::ImageUsageFlagBit::RENDER_TARGET, rhi::SampleCount::X8});
            rg.AddImage("ForwardDSMSAA", rdg::GraphImage{{width, height, 1}, 1, 1, depthStencilFormat, rhi::ImageUsageFlagBit::DEPTH_STENCIL, rhi::SampleCount::X8});
            rg.AddImage("BRDF_LUT", rdg::GraphImage{{BRDFLutSize, BRDFLutSize, 1}, 1, 1, rhi::PixelFormat::RGBA16_SFLOAT, rhi::ImageUsageFlagBit::RENDER_TARGET | rhi::ImageUsageFlagBit::SAMPLED, rhi::SampleCount::X8});

            auto lut = rdg.AddRasterPass("BRDF_LUT_PASS", 512, 512)
                .AddAttachment({"BRDF_LUT", rhi::LoadOp::DONT_CARE, rhi::StoreOp::STORE}, {});
            auto lutSub = lut.AddRasterSubPass("brdf_lut_sub0");
            lutSub.AddColor("BRDF_LUT", rdg::ResourceAccessBit::WRITE);
            lutSub.AddFullScreen("brdf_lut")
                .SetTechnique(brdfLutTech);

            auto forwardPass = rdg.AddRasterPass("forwardColor", width, height)
                                   .AddAttachment({"ForwardColor", rhi::LoadOp::CLEAR, rhi::StoreOp::STORE}, rhi::ClearValue(0.2f, 0.2f, 0.2f, 1.f))
                                   .AddAttachment({"ForwardColorMSAA", rhi::LoadOp::CLEAR, rhi::StoreOp::STORE}, rhi::ClearValue(0.2f, 0.2f, 0.2f, 1.f))
                                   .AddAttachment({"ForwardDSMSAA", rhi::LoadOp::CLEAR, rhi::StoreOp::STORE}, rhi::ClearValue(1.f, 0));

            auto subpass = forwardPass.AddRasterSubPass("color0_sub0");
            subpass.AddColor("ForwardColorMSAA", rdg::ResourceAccessBit::WRITE)
                .AddDepthStencil("ForwardDSMSAA", rdg::ResourceAccessBit::WRITE)
                .AddResolve("ForwardColor", rdg::ResourceAccessBit::WRITE)
                .AddComputeView("Irradiance", {"irradianceMap", rdg::ComputeType::SRV, rhi::ShaderStageFlagBit::FS})
                .AddComputeView("Radiance", {"radianceMap", rdg::ComputeType::SRV, rhi::ShaderStageFlagBit::FS})
                .AddComputeView("BRDF_LUT", {"brdfLutMap", rdg::ComputeType::SRV, rhi::ShaderStageFlagBit::FS})
                .AddComputeView(uboName, {"viewInfo", rdg::ComputeType::CBV, rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS})
                .AddComputeView(globalUboName, {"passInfo", rdg::ComputeType::CBV, rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS});

            subpass.AddQueue("queue1")
                .SetRasterID("ForwardColor")
                .SetView(sceneView)
                .SetLayout(forwardLayout);

            subpass.AddQueue("queue2")
                .SetRasterID("SkyBox")
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
        RDGfxTechPtr brdfLutTech;
        RDTextureCubePtr radiance;
        RDTextureCubePtr irradiance;

        RDUniformBufferPtr globalUbo;
        std::unique_ptr<SkyBoxRenderer> skybox;
    };

    bool SampleMesh::Start(sky::RenderWindow *window)
    {
        if (!SampleScene::Start(window)) {
            return false;
        }

        meshObj = world->CreateGameObject("Helmet");
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
        pipeline->SetOutput(window, GetRenderSceneFromGameObject(meshObj));

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
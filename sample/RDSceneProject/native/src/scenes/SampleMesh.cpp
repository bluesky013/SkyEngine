//
// Created by Zach Lee on 2023/9/16.
//

#include "SampleMesh.h"
#include "core/file/FileIO.h"
#include <render/RHI.h>
#include <render/RenderWindow.h>
#include <render/adaptor/Util.h>
#include <render/adaptor/assets/MeshAsset.h>
#include <render/adaptor/assets/ImageAsset.h>
#include <render/adaptor/components/CameraComponent.h>
#include <render/adaptor/components/MeshRenderer.h>
#include <rhi/Decode.h>

#include <framework/window/NativeWindow.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>

#include <render/rdg/RenderGraph.h>
#include <render/env/SkyBoxRenderer.h>
#include <render/mesh/GridRenderer.h>
#include <render/Renderer.h>

#include <imgui/ImGuiFeature.h>

#include "SimpleRotateComponent.h"

#include <RDSceneProject/ProjectRoot.h>

namespace sky {

//#define ENABLE_IBL

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
//                desc.bindings.emplace_back(rhi::DescriptorSetLayout::SetBinding {rhi::DescriptorType::UNIFORM_BUFFER, 1, 0, rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS, "passInfo"});
                desc.bindings.emplace_back(rhi::DescriptorSetLayout::SetBinding {rhi::DescriptorType::UNIFORM_BUFFER, 1, 1, rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS, "viewInfo"});
//                desc.bindings.emplace_back(rhi::DescriptorSetLayout::SetBinding {rhi::DescriptorType::SAMPLER, 1, 2, rhi::ShaderStageFlagBit::FS, "irradianceMap"});
//                desc.bindings.emplace_back(rhi::DescriptorSetLayout::SetBinding {rhi::DescriptorType::SAMPLER, 1, 3, rhi::ShaderStageFlagBit::FS, "radianceMap"});
//                desc.bindings.emplace_back(rhi::DescriptorSetLayout::SetBinding {rhi::DescriptorType::SAMPLER, 1, 4, rhi::ShaderStageFlagBit::FS, "brdfLutMap"});

                forwardLayout = std::make_shared<ResourceGroupLayout>();
                forwardLayout->SetRHILayout(rdgContext->device->CreateDescriptorSetLayout(desc));
//                forwardLayout->AddNameHandler("passInfo", {0, sizeof(ShaderPassInfo)});
                forwardLayout->AddNameHandler("viewInfo", {1, sizeof(SceneViewInfo)});
//                forwardLayout->AddNameHandler("irradianceMap", {2, 0});
//                forwardLayout->AddNameHandler("radianceMap", {3, 0});
//                forwardLayout->AddNameHandler("brdfLutMap", {4, 0});
            }

            auto *am = AssetManager::Get();
            postTech = am->LoadAsset<Technique>("techniques/post_processing.tech")->CreateInstanceAs<GraphicsTechnique>();
            brdfLutTech = am->LoadAsset<Technique>("techniques/brdf_lut.tech")->CreateInstanceAs<GraphicsTechnique>();

            irradiance = am->LoadAsset<Texture>("skybox/output_iem.dds")->CreateInstanceAs<TextureCube>();
            radiance = am->LoadAsset<Texture>("skybox/output_pmrem.dds")->CreateInstanceAs<TextureCube>();

//            auto skyboxTex = LoadCubeMap("/assets/skybox/output_skybox.dds");
//            auto skyboxMat = AssetManager::Get()->LoadAsset<Material>("materials/skybox.mat")->CreateInstance();
//            auto skyboxMatInst = std::make_shared<MaterialInstance>();
//            skyboxMatInst->SetMaterial(skyboxMat);
//            skyboxMatInst->SetTexture("skybox", skyboxTex, 0);
//            skyboxMatInst->Upload();

//            skybox = std::make_unique<SkyBoxRenderer>();
//            skybox->SetUp(skyboxMatInst);
//            skybox->AttachScene(scene);
        }

        bool OnSetup(rdg::RenderGraph &rdg, const std::vector<RenderScene*> &scenes) override
        {
            auto &rg = rdg.resourceGraph;

            const auto &views = scenes[0]->GetSceneViews();
            if (views.empty()) {
                return false;
            }

            if (gui == nullptr) {
                gui = ImGuiFeature::Get()->GetGuiInstance();
                if (gui != nullptr) {
                    scenes[0]->AddPrimitive(gui->GetPrimitive());
                }
            }

            if (gui != nullptr) {
                gui->Render(rdg);
            }


            auto *sceneView = views[0].get();
            const auto uboName = GetDefaultSceneViewUBOName(*sceneView);
            const auto *const globalUboName = "globalUBO";

            uint32_t renderWidth;
            uint32_t renderHeight;

            uint32_t outWidth;
            uint32_t outHeight;

            uint32_t viewCount = 1;
            const auto &swapChain = output->GetSwaChain();

            if (swapChain) {
                const auto &ext = swapChain->GetExtent();
                outWidth = ext.width;
                outHeight = ext.height;

                renderWidth  = ext.width * 2;
                renderHeight = ext.height * 2;
                rg.ImportSwapChain("SwapChain", swapChain);
            } else {
                auto xrSwapChain = output->GetXRSwaChain();
                const auto &ext = xrSwapChain->GetExtent();
                outWidth = ext.width;
                outHeight = ext.height;

                renderWidth  = ext.width * 2;
                renderHeight = ext.height * 2;

                viewCount = xrSwapChain->GetArrayLayers();
                std::vector<rhi::XRViewData> viewData;
                rhi::XRViewInput input = {0.01f, 100.f};
                if (!xrSwapChain->RequestViewData(input, viewData)) {
                    return false;
                }
                rg.ImportXRSwapChain("SwapChain", xrSwapChain);
            }


            ShaderPassInfo passInfo = {};
            passInfo.viewport.x = 0;
            passInfo.viewport.y = 0;
            passInfo.viewport.z = static_cast<float>(renderWidth);
            passInfo.viewport.w = static_cast<float>(renderHeight);
            globalUbo->Write(0, passInfo);

            rg.ImportUBO(uboName.c_str(), sceneView->GetUBO());
            rg.ImportUBO(globalUboName, globalUbo);

            rhi::PixelFormat hdrFormat = rhi::PixelFormat::RGBA16_SFLOAT;
            const uint32_t BRDFLutSize = 512;

            auto viewType = viewCount > 1 ? rhi::ImageViewType::VIEW_2D_ARRAY : rhi::ImageViewType::VIEW_2D;

            rg.ImportImage("Radiance", radiance->GetImage(), radiance->GetImageView()->GetViewDesc().viewType);
            rg.ImportImage("Irradiance", irradiance->GetImage(), radiance->GetImageView()->GetViewDesc().viewType);
            rg.AddImage("ForwardColor", rdg::GraphImage{{renderWidth, renderHeight, 1}, 1, viewCount, hdrFormat, rhi::ImageUsageFlagBit::RENDER_TARGET | rhi::ImageUsageFlagBit::SAMPLED, rhi::SampleCount::X1, viewType});
            rg.AddImage("ForwardColorMSAA", rdg::GraphImage{{renderWidth, renderHeight, 1}, 1, viewCount, hdrFormat, rhi::ImageUsageFlagBit::RENDER_TARGET, rhi::SampleCount::X2, viewType});
            rg.AddImage("ForwardDSMSAA", rdg::GraphImage{{renderWidth, renderHeight, 1}, 1, viewCount, depthStencilFormat, rhi::ImageUsageFlagBit::DEPTH_STENCIL, rhi::SampleCount::X2, viewType});
#ifdef ENABLE_IBL
            rg.AddImage("BRDF_LUT", rdg::GraphImage{{BRDFLutSize, BRDFLutSize, 1}, 1, 1, rhi::PixelFormat::RGBA16_SFLOAT, rhi::ImageUsageFlagBit::RENDER_TARGET | rhi::ImageUsageFlagBit::SAMPLED});

            auto lut = rdg.AddRasterPass("BRDF_LUT_PASS", 512, 512)
                .AddAttachment({"BRDF_LUT", rhi::LoadOp::DONT_CARE, rhi::StoreOp::STORE}, {});
            auto lutSub = lut.AddRasterSubPass("brdf_lut_sub0");
            lutSub.AddColor("BRDF_LUT", rdg::ResourceAccessBit::WRITE);
            lutSub.AddFullScreen("brdf_lut")
                .SetTechnique(brdfLutTech);
#endif

            auto forwardPass = rdg.AddRasterPass("forwardColor", renderWidth, renderHeight)
                                   .AddAttachment({"ForwardColor", rhi::LoadOp::CLEAR, rhi::StoreOp::STORE}, rhi::ClearValue(0.2f, 0.2f, 0.2f, 1.f))
                                   .AddAttachment({"ForwardColorMSAA", rhi::LoadOp::CLEAR, rhi::StoreOp::STORE}, rhi::ClearValue(0.2f, 0.2f, 0.2f, 1.f))
                                   .AddAttachment({"ForwardDSMSAA", rhi::LoadOp::CLEAR, rhi::StoreOp::STORE}, rhi::ClearValue(1.f, 0));
//            forwardPass.AddCoRelationMasks(viewMask);

            auto subpass = forwardPass.AddRasterSubPass("color0_sub0");
            subpass.AddColor("ForwardColorMSAA", rdg::ResourceAccessBit::WRITE)
                .AddDepthStencil("ForwardDSMSAA", rdg::ResourceAccessBit::WRITE)
                .AddResolve("ForwardColor", rdg::ResourceAccessBit::WRITE)
                .AddComputeView("Irradiance", {"irradianceMap", rdg::ComputeType::SRV, rhi::ShaderStageFlagBit::FS})
                .AddComputeView("Radiance", {"radianceMap", rdg::ComputeType::SRV, rhi::ShaderStageFlagBit::FS})
#ifdef ENABLE_IBL
                .AddComputeView("BRDF_LUT", {"brdfLutMap", rdg::ComputeType::SRV, rhi::ShaderStageFlagBit::FS})
#endif
                .AddComputeView(uboName, {"viewInfo", rdg::ComputeType::CBV, rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS})
                .AddComputeView(globalUboName, {"passInfo", rdg::ComputeType::CBV, rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS});

            subpass.SetViewMask(viewMask);

            subpass.AddQueue("queue1")
                .SetRasterID("ForwardColor")
                .SetView(sceneView)
                .SetLayout(forwardLayout);

            subpass.AddQueue("queue2")
                .SetRasterID("SkyBox")
                .SetView(sceneView)
                .SetLayout(forwardLayout);

            auto pp = rdg.AddRasterPass("PostProcessing", outWidth, outHeight)
                      .AddAttachment({"SwapChain", rhi::LoadOp::DONT_CARE, rhi::StoreOp::STORE}, {});
            auto ppSub = pp.AddRasterSubPass("pp_sub0");
            ppSub.AddColor("SwapChain", rdg::ResourceAccessBit::WRITE)
                .AddComputeView("ForwardColor", {"InColor", rdg::ComputeType::SRV, rhi::ShaderStageFlagBit::FS});
            ppSub.AddFullScreen("fullscreen")
                .SetTechnique(postTech);

            ppSub.AddQueue("queue").SetRasterID("ui");
            rdg.AddPresentPass("present", "SwapChain");

            return true;
        }

    private:
        uint32_t viewMask = 0; // 0b11;
        RenderWindow *output = nullptr;
        rhi::PixelFormat depthStencilFormat = rhi::PixelFormat::D24_S8;
        RDResourceLayoutPtr forwardLayout;
        RDGfxTechPtr postTech;
        RDGfxTechPtr brdfLutTech;
        RDTextureCubePtr radiance;
        RDTextureCubePtr irradiance;

        ImGuiInstance *gui = nullptr;
        RDUniformBufferPtr globalUbo;
        std::unique_ptr<SkyBoxRenderer> skybox;
    };

    bool SampleMesh::Start(sky::RenderWindow *window)
    {
        if (!SampleScene::Start(window)) {
            return false;
        }

        meshObj = world->CreateActor("Helmet");
        meshObj->GetComponent<TransformComponent>()->SetLocalRotation(
            Quaternion(-30 / 180.f * 3.14f, VEC3_Y) *
            Quaternion(90 / 180.f * 3.14f, VEC3_X));
        meshObj->AddComponent<SimpleRotateComponent>();

        auto *mesh = meshObj->AddComponent<MeshRenderer>();
        mesh->SetMesh(AssetManager::Get()->LoadAsset<Mesh>("models/DamagedHelmet.glb_node_damagedHelmet_-6514_mesh"));

//        auto mat = AssetManager::Get()->LoadAsset<MaterialInstance>("materials/floor.mati")->CreateInstance();
//        auto floor = GridRenderer().SetUp({512}).BuildMesh(mat);
//        mesh->SetMesh(floor);

        camera = world->CreateActor("MainCamera");
        auto *cc = camera->AddComponent<CameraComponent>();
        cc->Perspective(0.01f, 100.f, 45.f / 180.f * 3.14f);
        cc->SetAspect(window->GetWidth(), window->GetHeight());
        camera->GetComponent<TransformComponent>()->SetWorldTranslation(Vector3(0, 0.5, 5));

        auto *scene = GetRenderSceneFromGameObject(meshObj);
        auto *pipeline = new ForwardMSAAPass();
        pipeline->SetOutput(window, GetRenderSceneFromGameObject(meshObj));

        Renderer::Get()->SetPipeline(pipeline);
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
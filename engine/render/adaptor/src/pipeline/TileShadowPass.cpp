//
// Created by blues on 2024/9/6.
//

#include <render/adaptor/pipeline/TileShadowPass.h>
#include <render/adaptor/Util.h>

#include <render/rdg/RenderGraph.h>
#include <render/RHI.h>
#include <render/RenderScene.h>
#include <render/RenderScenePipeline.h>
#include <render/Renderer.h>
#include <render/light/LightFeatureProcessor.h>
#include <rhi/Decode.h>

#include <algorithm>
#include <string>
#include <cstring>

namespace sky {

    // ─── ShadowSlotPass ───────────────────────────────────────────────────────

    ShadowSlotPass::ShadowSlotPass(uint32_t index, uint32_t shadowMapSize)
        : RasterPass(Name(("TileShadow_" + std::to_string(index)).c_str()))
        , slotIndex(index)
    {
        width  = shadowMapSize;
        height = shadowMapSize;

        const std::string id = std::to_string(index);
        layerName     = Name(("TileShadowLayer_"     + id).c_str());
        viewUBOName   = Name(("TileShadowViewUBO_"   + id).c_str());
        sceneViewName = Name(("TileShadowSceneView_" + id).c_str());

        auto stageFlags = rhi::ShaderStageFlagBit::VS  | rhi::ShaderStageFlagBit::FS |
                          rhi::ShaderStageFlagBit::TAS | rhi::ShaderStageFlagBit::MS;

        // Bind the same pass-info UBO that all other passes use (contains light matrix etc.)
        computeResources.emplace_back(ComputeResource{
            Name("FWD_PassInfo"),
            rdg::ComputeView{Name("passInfo"), rdg::ComputeType::CBV, stageFlags}
        });

        // Bind the per-light shadow view UBO
        computeResources.emplace_back(ComputeResource{
            viewUBOName,
            rdg::ComputeView{Name("viewInfo"), rdg::ComputeType::CBV, stageFlags}
        });
    }

    void ShadowSlotPass::SetLayout(const RDResourceLayoutPtr &layout_)
    {
        layout = layout_;
    }

    void ShadowSlotPass::SetShadowView(SceneView *view)
    {
        shadowSceneView = view;
    }

    void ShadowSlotPass::Setup(rdg::RenderGraph &rdg, RenderScene &scene)
    {
        if (shadowSceneView == nullptr) {
            return; // No active light for this slot – skip rendering
        }

        // Use the atlas layer view that TileShadowPass imported this frame
        depthStencil = Attachment{
            rdg::RasterAttachment{layerName, rhi::LoadOp::CLEAR, rhi::StoreOp::STORE},
            rhi::ClearValue(1.f, 0)
        };

        // Make the shadow-view UBO available in the render graph
        rdg.resourceGraph.ImportUBO(viewUBOName, shadowSceneView->GetUBO());

        RasterPass::Setup(rdg, scene);
    }

    void ShadowSlotPass::SetupSubPass(rdg::RasterSubPassBuilder &builder, RenderScene &/*scene*/)
    {
        builder.SetViewMask(0);
        builder.AddQueue(Name("queue1"))
            .SetRasterID(Name("Shadow"))
            .SetSceneView(sceneViewName)
            .SetLayout(layout);
    }

    // ─── TileShadowPass ───────────────────────────────────────────────────────

    TileShadowPass::TileShadowPass()
    {
        for (uint32_t i = 0; i < MAX_LIGHTS; ++i) {
            slotPasses[i] = std::make_unique<ShadowSlotPass>(i, MAP_SIZE);
        }

        shadowInfoUBO = new UniformBuffer();
        shadowInfoUBO->Init(sizeof(TileShadowPassInfo));

        tileBitmaskUBO = new UniformBuffer();
        tileBitmaskUBO->Init(TILE_SHADOW_MAX_TILES * sizeof(uint32_t));

        // Zero-initialise CPU shadow info
        std::memset(&shadowInfo, 0, sizeof(shadowInfo));
    }

    void TileShadowPass::SetLayout(const RDResourceLayoutPtr &layout_)
    {
        layout = layout_;
        for (auto &pass : slotPasses) {
            pass->SetLayout(layout_);
        }
    }

    // ── EnsureAtlas ──────────────────────────────────────────────────────────

    void TileShadowPass::EnsureAtlas()
    {
        if (atlasImage) {
            return;
        }

        rhi::Image::Descriptor desc = {};
        desc.imageType   = rhi::ImageType::IMAGE_2D;
        desc.format      = rhi::PixelFormat::D32;
        desc.extent      = {MAP_SIZE, MAP_SIZE, 1};
        desc.mipLevels   = 1;
        desc.arrayLayers = MAX_LIGHTS;
        desc.samples     = rhi::SampleCount::X1;
        desc.usage       = rhi::ImageUsageFlagBit::DEPTH_STENCIL | rhi::ImageUsageFlagBit::SAMPLED;
        desc.memory      = rhi::MemoryType::GPU_ONLY;

        atlasImage = RHI::Get()->GetDevice()->CreateImage(desc);

        // Full array view for shader sampling
        {
            rhi::ImageViewDesc viewDesc = {};
            viewDesc.viewType = rhi::ImageViewType::VIEW_2D_ARRAY;
            viewDesc.subRange = {0, 1, 0, MAX_LIGHTS, rhi::AspectFlagBit::DEPTH_BIT};
            atlasFullView = atlasImage->CreateView(viewDesc);
        }

        // One view per layer for rendering
        for (uint32_t i = 0; i < MAX_LIGHTS; ++i) {
            rhi::ImageViewDesc layerDesc = {};
            layerDesc.viewType = rhi::ImageViewType::VIEW_2D;
            layerDesc.subRange = {0, 1, i, 1, rhi::AspectFlagBit::DEPTH_BIT};
            atlasLayerViews[i] = atlasImage->CreateView(layerDesc);
        }
    }

    // ── UpdateLightData ──────────────────────────────────────────────────────

    void TileShadowPass::UpdateLightData(RenderScene &scene)
    {
        auto *lf = GetFeatureProcessor<LightFeatureProcessor>(&scene);

        activeLights = 0;
        std::memset(&shadowInfo, 0, sizeof(shadowInfo));

        if (lf == nullptr) {
            shadowInfoUBO->WriteT(0, shadowInfo);
            return;
        }

        // Slot 0: main directional light
        auto *mainLight = lf->GetMainLight();
        if (mainLight != nullptr) {
            const uint32_t idx = activeLights;

            if (shadowViews[idx] == nullptr) {
                shadowViews[idx] = scene.CreateSceneView(1);
                scene.AttachSceneView(
                    shadowViews[idx],
                    Name(("TileShadowSceneView_" + std::to_string(idx)).c_str()));
            }

            mainLight->BuildMatrix(*shadowViews[idx]);
            shadowViews[idx]->Update();

            ShadowLightData &ld = shadowInfo.lights[idx];
            ld.lightViewProj    = mainLight->GetMatrix();
            ld.posRadius        = Vector4(0.f, 0.f, 0.f, 0.f);
            ld.lightType        = 0; // directional
            std::memset(ld.pad, 0, sizeof(ld.pad));

            slotPasses[idx]->SetShadowView(shadowViews[idx]);
            ++activeLights;
        }

        // Slots 1–3: spot lights from the feature processor's light list
        const auto &lights = lf->GetLights();
        for (auto it = lights.begin(); it != lights.end() && activeLights < MAX_LIGHTS; ++it) {
            auto *spotLight = dynamic_cast<SpotLight *>(it->get());
            if (spotLight == nullptr) {
                continue;
            }

            const uint32_t idx = activeLights;

            if (shadowViews[idx] == nullptr) {
                shadowViews[idx] = scene.CreateSceneView(1);
                scene.AttachSceneView(
                    shadowViews[idx],
                    Name(("TileShadowSceneView_" + std::to_string(idx)).c_str()));
            }

            // Build spot-light shadow matrix
            spotLight->BuildShadowMatrix(*shadowViews[idx]);
            shadowViews[idx]->Update();

            ShadowLightData &ld = shadowInfo.lights[idx];
            ld.lightViewProj    = shadowViews[idx]->GetViewProject();
            ld.lightType        = 1; // spot

            LightInfo li{};
            spotLight->Collect(li);
            ld.posRadius = Vector4(li.position.x, li.position.y, li.position.z,
                                   spotLight->GetRange());
            std::memset(ld.pad, 0, sizeof(ld.pad));

            slotPasses[idx]->SetShadowView(shadowViews[idx]);
            ++activeLights;
        }

        // Disable inactive slots
        for (uint32_t i = activeLights; i < MAX_LIGHTS; ++i) {
            slotPasses[i]->SetShadowView(nullptr);
        }

        shadowInfo.lightCount = activeLights;
        shadowInfoUBO->WriteT(0, shadowInfo);
    }

    // ── BuildTileBitmask ─────────────────────────────────────────────────────

    void TileShadowPass::BuildTileBitmask(const RenderScene &scene, uint32_t screenW, uint32_t screenH)
    {
        uint32_t tileCountX = (screenW + TILE_SIZE - 1) / TILE_SIZE;
        uint32_t tileCountY = (screenH + TILE_SIZE - 1) / TILE_SIZE;
        uint32_t totalTiles = std::min(tileCountX * tileCountY, TILE_SHADOW_MAX_TILES);

        // Write tile counts back into the info UBO
        shadowInfo.tileCountX = tileCountX;
        shadowInfo.tileCountY = tileCountY;
        shadowInfoUBO->WriteT(0, shadowInfo);

        // Build per-tile bitmask (bit N = shadow light N affects tile)
        std::vector<uint32_t> bitmasks(TILE_SHADOW_MAX_TILES, 0u);

        if (activeLights == 0) {
            tileBitmaskUBO->Write(0,
                reinterpret_cast<const uint8_t *>(bitmasks.data()),
                static_cast<uint32_t>(TILE_SHADOW_MAX_TILES * sizeof(uint32_t)));
            return;
        }

        auto *cameraView = scene.GetSceneView(Name("MainCamera"));
        const Matrix4 *viewProjPtr = (cameraView != nullptr) ? &cameraView->GetViewProject() : nullptr;

        for (uint32_t li = 0; li < activeLights; ++li) {
            const ShadowLightData &ld = shadowInfo.lights[li];

            if (ld.lightType == 0) {
                // Directional light: affects every tile
                for (uint32_t t = 0; t < totalTiles; ++t) {
                    bitmasks[t] |= (1u << li);
                }
            } else if (viewProjPtr != nullptr) {
                // Spot/point light: project bounding sphere into screen space
                const Vector3 worldPos(ld.posRadius.x, ld.posRadius.y, ld.posRadius.z);
                const float   radius = ld.posRadius.w;

                Vector4 clipPos = (*viewProjPtr) * Vector4(worldPos.x, worldPos.y, worldPos.z, 1.f);
                if (clipPos.w <= 0.f) {
                    continue;
                }

                float ndcX = clipPos.x / clipPos.w;
                float ndcY = clipPos.y / clipPos.w;
                // Approximate screen-space radius
                float screenRadius = radius / clipPos.w * static_cast<float>(screenW) * 0.5f;

                float sx = (ndcX * 0.5f + 0.5f) * static_cast<float>(screenW);
                float sy = (ndcY * 0.5f + 0.5f) * static_cast<float>(screenH);

                int minTX = static_cast<int>((sx - screenRadius) / static_cast<float>(TILE_SIZE));
                int maxTX = static_cast<int>((sx + screenRadius) / static_cast<float>(TILE_SIZE));
                int minTY = static_cast<int>((sy - screenRadius) / static_cast<float>(TILE_SIZE));
                int maxTY = static_cast<int>((sy + screenRadius) / static_cast<float>(TILE_SIZE));

                minTX = std::max(0, minTX);
                maxTX = std::min(static_cast<int>(tileCountX) - 1, maxTX);
                minTY = std::max(0, minTY);
                maxTY = std::min(static_cast<int>(tileCountY) - 1, maxTY);

                for (int ty = minTY; ty <= maxTY; ++ty) {
                    for (int tx = minTX; tx <= maxTX; ++tx) {
                        uint32_t tileIdx = static_cast<uint32_t>(ty) * tileCountX +
                                           static_cast<uint32_t>(tx);
                        if (tileIdx < TILE_SHADOW_MAX_TILES) {
                            bitmasks[tileIdx] |= (1u << li);
                        }
                    }
                }
            }
        }

        tileBitmaskUBO->Write(0,
            reinterpret_cast<const uint8_t *>(bitmasks.data()),
            static_cast<uint32_t>(TILE_SHADOW_MAX_TILES * sizeof(uint32_t)));
    }

    // ── Setup ─────────────────────────────────────────────────────────────────

    void TileShadowPass::Setup(rdg::RenderGraph &rdg, RenderScene &scene,
                               uint32_t screenW, uint32_t screenH)
    {
        EnsureAtlas();
        UpdateLightData(scene);
        BuildTileBitmask(scene, screenW, screenH);

        auto &rsg = rdg.resourceGraph;

        // Import per-layer views so each ShadowSlotPass can write to its layer.
        // Use NONE as the initial state each frame – the render graph will generate
        // the required barrier from the previous state.
        for (uint32_t i = 0; i < MAX_LIGHTS; ++i) {
            rsg.ImportImageView(
                Name(("TileShadowLayer_" + std::to_string(i)).c_str()),
                atlasImage, atlasLayerViews[i],
                rhi::AccessFlagBit::NONE);
        }

        // Import full array view for shader sampling.
        // After the slot passes run, the atlas layers will be in DEPTH_STENCIL_WRITE
        // state; the render graph will transition them to FRAGMENT_SRV as needed.
        rsg.ImportImageView(Name(TILE_SHADOW_ATLAS.data()), atlasImage, atlasFullView,
                            rhi::AccessFlagBit::NONE);

        // Import UBOs
        rsg.ImportUBO(Name(TILE_SHADOW_INFO.data()),   shadowInfoUBO);
        rsg.ImportUBO(Name(TILE_LIGHT_BITMASK.data()), tileBitmaskUBO);

        // Register shadow scene views with the render graph
        for (uint32_t i = 0; i < activeLights; ++i) {
            if (shadowViews[i] != nullptr) {
                rdg.AddSceneView(
                    Name(("TileShadowSceneView_" + std::to_string(i)).c_str()),
                    shadowViews[i]);
            }
        }
    }

    // ── AddPasses ─────────────────────────────────────────────────────────────

    void TileShadowPass::AddPasses(RenderScenePipeline &pipeline)
    {
        for (uint32_t i = 0; i < activeLights; ++i) {
            pipeline.AddPass(slotPasses[i].get());
        }
    }

} // namespace sky

//
// Created by blues on 2024/9/6.
//

#pragma once

#include <render/renderpass/RasterPass.h>
#include <render/adaptor/pipeline/DefaultPassConstants.h>
#include <render/resource/Buffer.h>
#include <render/RenderBuiltinLayout.h>
#include <array>

namespace sky {
    class RenderScenePipeline;
    class SceneView;

    /**
     * @brief Raster pass that renders one shadow-casting light into a single layer of the shadow atlas.
     *
     * Each ShadowSlotPass is managed by TileShadowPass.  The atlas layer image view is
     * imported into the render graph by TileShadowPass before this pass runs.
     */
    class ShadowSlotPass : public RasterPass {
    public:
        explicit ShadowSlotPass(uint32_t index, uint32_t shadowMapSize);
        ~ShadowSlotPass() override = default;

        void SetLayout(const RDResourceLayoutPtr &layout_);
        void SetShadowView(SceneView *view);
    private:
        void Setup(rdg::RenderGraph &rdg, RenderScene &scene) override;
        void SetupSubPass(rdg::RasterSubPassBuilder &builder, RenderScene &scene) override;

        uint32_t  slotIndex;
        Name      layerName;      // Imported atlas-layer resource in the render graph
        Name      viewUBOName;    // Shadow-view UBO resource in the render graph
        Name      sceneViewName;  // Name under which the SceneView is registered
        SceneView *shadowSceneView = nullptr;
    };

    /**
     * @brief Tile-based shadow map manager.
     *
     * Owns a persistent Texture2DArray shadow atlas (one layer per active shadow light).
     * Maintains:
     *   - ShadowSlotPass instances to render each light's shadow map.
     *   - A TileShadowPassInfo UBO with per-light view-projection matrices.
     *   - A per-tile bitmask UBO that records which shadow lights affect each screen tile.
     *
     * The bitmask is computed on the CPU each frame and is used by the fragment shader to
     * skip lights that do not affect a given tile.
     *
     * Usage in the pipeline (inside DefaultForwardPipeline::Collect):
     * @code
     *   tileShadow->Setup(rdg, *scene, width, height);
     *   tileShadow->AddPasses(*this);
     * @endcode
     */
    class TileShadowPass {
    public:
        static constexpr uint32_t MAX_LIGHTS = TILE_SHADOW_MAX_LIGHTS;
        static constexpr uint32_t MAP_SIZE   = 1024;
        static constexpr uint32_t TILE_SIZE  = TILE_SHADOW_TILE_SIZE;

        TileShadowPass();
        ~TileShadowPass() = default;

        void SetLayout(const RDResourceLayoutPtr &layout_);

        /**
         * @brief Called from DefaultForwardPipeline::Collect.
         *        Imports persistent GPU resources and updates CPU-side data.
         */
        void Setup(rdg::RenderGraph &rdg, RenderScene &scene, uint32_t screenW, uint32_t screenH);

        /**
         * @brief Registers active ShadowSlotPass instances with the pipeline.
         *        Must be called after Setup().
         */
        void AddPasses(RenderScenePipeline &pipeline);

    private:
        void EnsureAtlas();
        void UpdateLightData(RenderScene &scene);
        void BuildTileBitmask(const RenderScene &scene, uint32_t screenW, uint32_t screenH);

        // Slot passes (one per shadow light)
        std::array<std::unique_ptr<ShadowSlotPass>, MAX_LIGHTS> slotPasses;

        // Shadow scene views (created lazily, owned by RenderScene)
        std::array<SceneView *, MAX_LIGHTS> shadowViews{};

        // Persistent GPU shadow atlas
        rhi::ImagePtr     atlasImage;
        rhi::ImageViewPtr atlasFullView;   // Full Texture2DArray view for shader sampling
        std::array<rhi::ImageViewPtr, MAX_LIGHTS> atlasLayerViews; // Per-layer views for rendering

        // Per-frame CPU data stored in UBOs
        RDUniformBufferPtr shadowInfoUBO;  // TileShadowPassInfo
        RDUniformBufferPtr tileBitmaskUBO; // uint32_t[TILE_SHADOW_MAX_TILES]

        // CPU-side copy so we can read back values without extra UBO mapping
        TileShadowPassInfo shadowInfo{};
        uint32_t activeLights = 0;

        // Whether the atlas image has been rendered to at least once (for initial barrier state)
        bool atlasInitialized = false;

        RDResourceLayoutPtr layout;
    };

} // namespace sky

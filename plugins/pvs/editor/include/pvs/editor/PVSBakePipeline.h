//
// Created by Zach Lee on 2026/2/27.
//

#pragma once

#include <render/RenderScenePipeline.h>
#include <render/rdg/RenderGraph.h>
#include <render/renderpass/RasterPass.h>
#include <render/FeatureProcessor.h>
#include <pvs/editor/PVSBoxPrimitive.h>
#include <pvs/editor/PVSWorldBuilder.h>

namespace sky::editor {

    class PVSDrawIDPass : public RasterPass {
    public:
        explicit PVSDrawIDPass(uint32_t inWidth, uint32_t inHeight);

        ~PVSDrawIDPass() override = default;

    private:
        void SetupSubPass(rdg::RasterSubPassBuilder& builder, RenderScene &scene) override;

        rhi::PixelFormat colorFormat     = rhi::PixelFormat::U32;
        rhi::PixelFormat depthStenFormat = rhi::PixelFormat::D32;

        RDResourceLayoutPtr layout;
    };

    class PVSPipeline : public RenderScenePipeline {
    public:
        explicit PVSPipeline(RenderScene *scn, uint32_t bakeWidth, uint32_t bakeHeight)
            : RenderScenePipeline(scn)
            , pass(std::make_unique<PVSDrawIDPass>(bakeWidth, bakeHeight))
        {
        }
        ~PVSPipeline() override = default;

        void Collect(rdg::RenderGraph &rdg) override
        {
            AddPass(pass.get());
        }
    private:
        std::unique_ptr<PVSDrawIDPass> pass;
    };

    struct PVSCellReadBackTask {
        PVSCellReadBackTask(uint32_t cellIndex, uint32_t width, uint32_t height, uint32_t layers);

        void Execute();

        uint32_t cellIndex;
        std::shared_ptr<PVSBuildContext> context;

        rhi::ImagePtr image;
        std::vector<rhi::ImageViewPtr> imageViews;
    };

    class PVSDrawFeatureProcessor : public IFeatureProcessor {
    public:
        explicit PVSDrawFeatureProcessor(RenderScene *scene);
        ~PVSDrawFeatureProcessor() override = default;

        void ResetBuildTask(std::vector<CellBuildTask>&& tasks, const std::shared_ptr<PVSBuildContext> context);

        void Tick(float time) override;
        void Render(rdg::RenderGraph &rdg) override;

        SceneView* view;

        uint32_t bakeWidth = 1024;
        uint32_t bakeHeight = 1024;

        uint32_t currentTask = ~(0U);
        uint32_t taskNum = 0;
        uint32_t sampleNum = 0;
        std::shared_ptr<PVSBuildContext> context;
        std::vector<CellBuildTask> cellBuildTask;
        std::unique_ptr<PVSGeometryPrimitive> primitive;
        std::unique_ptr<PVSPipeline> pipeline;

        std::shared_ptr<PVSCellReadBackTask> readbackTask;
    };


} // namespace sky::editor
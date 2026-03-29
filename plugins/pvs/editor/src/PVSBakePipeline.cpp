//
// Created by Zach Lee on 2026/2/27.
//

#include "framework/asset/AssetDataBase.h"

#include <core/math/Transform.h>
#include <framework/asset/AssetManager.h>
#include <pvs/editor/PVSBakePipeline.h>
#include <render/RHI.h>
#include <render/RenderScene.h>
#include <render/adaptor/assets/TechniqueAsset.h>

namespace sky::editor {
    PVSDrawIDPass::PVSDrawIDPass(uint32_t inWidth, uint32_t inHeight) : RasterPass(Name("PVSDrawIDPass"))
    {
        Name pvsColor("PvsID");
        Name pvsDepth("PvsDepth");
        Name pvsView("PvsView");

        width = inWidth;
        height = inHeight;

        rdg::GraphImage image = {};
        image.extent.width  = width;
        image.extent.height = height;
        image.format        = colorFormat;
        image.usage   = rhi::ImageUsageFlagBit::DEPTH_STENCIL;
        image.format  = depthStenFormat;
        images.emplace_back(pvsDepth, image);

        colors.emplace_back(
            Attachment{rdg::RasterAttachment{pvsColor, rhi::LoadOp::CLEAR, rhi::StoreOp::STORE}, rhi::ClearValue(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF)});

        depthStencil =
            Attachment{rdg::RasterAttachment{pvsDepth, rhi::LoadOp::CLEAR, rhi::StoreOp::STORE}, rhi::ClearValue(1.f, 0)};

        rhi::DescriptorSetLayout::Descriptor desc = {};
        auto stageFlags = rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS | rhi::ShaderStageFlagBit::TAS | rhi::ShaderStageFlagBit::MS;
        desc.bindings.emplace_back(
            rhi::DescriptorType::UNIFORM_BUFFER, 1, 0, stageFlags, "viewInfo");

        computeResources.emplace_back(ComputeResource{
            Name(pvsView),
            rdg::ComputeView{Name("viewInfo"), rdg::ComputeType::CBV, stageFlags}
        });

        layout = new ResourceGroupLayout();
        layout->SetRHILayout(RHI::Get()->GetDevice()->CreateDescriptorSetLayout(desc));
        layout->AddNameHandler(Name("viewInfo"), {0, sizeof(SceneViewInfo)});
    }

    void PVSDrawIDPass::SetupSubPass(rdg::RasterSubPassBuilder& builder, RenderScene &scene)
    {
        const Name viewName("PVSSampleCamera");

        builder.AddQueue(Name("queue1"))
            .SetRasterID(Name("occlusion"))
            .SetSceneView(viewName)
            .SetLayout(layout);
    }

    PVSDrawFeatureProcessor::PVSDrawFeatureProcessor(RenderScene *scene)
        : IFeatureProcessor(scene)
    {
        view = scene->CreateSceneView(1);
        pipeline = std::make_unique<PVSPipeline>(scene, bakeWidth, bakeHeight);

        auto guiAsset = AssetManager::Get()->LoadAssetFromPath<Technique>("techniques/draw_id.tech");
        guiAsset->BlockUntilLoaded();
        primitive = std::make_unique<PVSGeometryPrimitive>(CreateTechniqueFromAsset(guiAsset));
        scene->AddPrimitive(primitive.get());
    }

    void PVSDrawFeatureProcessor::ResetBuildTask(std::vector<CellBuildTask>&& tasks, const std::shared_ptr<PVSBuildContext> inContext)
    {
        cellBuildTask.swap(tasks);
        if (!cellBuildTask.empty()) {
            currentTask = 0;
            context = inContext;
            sampleNum = static_cast<uint32_t>(context->samples.size());
            taskNum = static_cast<uint32_t>(cellBuildTask.size() * sampleNum);
            primitive->Update(context->objectGeometryInstance);
            scene->AttachSceneView(view, Name("PVSSampleCamera"));
        }
    }

    void PVSDrawFeatureProcessor::Tick(float time)
    {
        if (readbackTask) {
            uint32_t sampleID = currentTask % sampleNum;
            if (sampleID == 0) {
                readbackTask->Execute();
                readbackTask.reset();
            }
        }

        if (context && currentTask >= taskNum) {
            context->Save();
            context.reset();

            currentTask = ~(0U);
            taskNum = 0;
        }
    }

    void PVSDrawFeatureProcessor::Render(rdg::RenderGraph &rdg)
    {
        if (currentTask >= taskNum) {
            return;
        }

        // build view param
        uint32_t cellIdx = currentTask / sampleNum;
        uint32_t sampleID = currentTask % sampleNum;

        if (sampleID == 0) {
            readbackTask = std::make_shared<PVSCellReadBackTask>(cellIdx, bakeWidth, bakeHeight, sampleNum);
            readbackTask->context = context;
            readbackTask->cellIndex = cellIdx;
        }

        const auto &task = cellBuildTask[cellIdx];
        const auto &cell = context->sortedCells[task.cellID];
        const auto &sampleParam = context->samples[sampleID];
        const auto &config = context->config;

        // World position: cell min + local normalized position * cell extent
        Vector3 cellExtent(config.cellSize, config.cellSizeY, config.cellSize);
        Vector3 worldPos = cell + sampleParam.localPositionInCell * cellExtent;

        // Build rotation: camera looks along -Z, so we need -Z -> direction
        // i.e. the camera's forward (-Z) should equal sampleParam.direction
        // => the camera's +Z axis (third column of world matrix) = -direction
        Vector3 forward = sampleParam.direction;
        forward.Normalize();

        // Choose an up hint that is not parallel to forward
        Vector3 upHint = (std::fabs(forward.y) < 0.999f) ? VEC3_Y : VEC3_X;

        Vector3 right = upHint.Cross(forward);
        right.Normalize();

        Vector3 up = forward.Cross(right);
        // up is already unit length since forward and right are orthonormal

        // Construct world matrix (column-major storage, each m[i] is a column)
        // Column 0 = right (X axis)
        // Column 1 = up    (Y axis)
        // Column 2 = -forward (Z axis, camera looks along -Z)
        // Column 3 = translation
        Matrix4 worldMatrix = Matrix4::Identity();
        worldMatrix[0] = Vector4(right.x,    right.y,    right.z,    0.f);
        worldMatrix[1] = Vector4(up.x,       up.y,       up.z,       0.f);
        worldMatrix[2] = Vector4(-forward.x, -forward.y, -forward.z, 0.f);
        worldMatrix[3] = Vector4(worldPos.x, worldPos.y, worldPos.z, 1.f);

        auto *mainView = scene->GetSceneView(Name("MainCamera"));
        view->SetMatrix(worldMatrix);
        view->SetPerspective(mainView->GetNearPlane(), mainView->GetFarPlane(), 120.f / 180.f * PI, 1.f);
        view->Update();

        mainView->SetMatrix(worldMatrix);
        mainView->Update();

        rdg::GraphImageView graphView = {};
        graphView.view.viewType = rhi::ImageViewType::VIEW_2D;
        graphView.view.subRange.aspectMask = rhi::AspectFlagBit::COLOR_BIT;
        graphView.view.subRange.baseLevel = 0;
        graphView.view.subRange.levels    = 1;
        graphView.view.subRange.baseLayer = sampleID;
        graphView.view.subRange.layers    = 1;

        rdg.resourceGraph.ImportUBO(Name("PvsView"), view->GetUBO());
        rdg.resourceGraph.ImportImageView(Name("PvsID"), readbackTask->image, readbackTask->imageViews[sampleID], rhi::AccessFlagBit::NONE);
        pipeline->Setup(rdg);

        ++currentTask;
    }

    PVSCellReadBackTask::PVSCellReadBackTask(uint32_t InIndex, uint32_t width, uint32_t height, uint32_t layers)
        : cellIndex(InIndex)
    {
        auto *device = RHI::Get()->GetDevice();
        rhi::Image::Descriptor imageDesc = {
            .format = rhi::PixelFormat::U32,
            .extent = {width, height, 1},
            .arrayLayers = layers,
            .usage = rhi::ImageUsageFlagBit::RENDER_TARGET | rhi::ImageUsageFlagBit::TRANSFER_SRC
        };
        image = device->CreateImage(imageDesc);
        imageViews.resize(layers);

        rhi::ImageViewDesc viewDesc = {};
        for (uint32_t i = 0; i < layers; i++) {
            viewDesc.subRange.baseLayer = i;
            imageViews[i] = image->CreateView(viewDesc);
        }
    }

    void PVSCellReadBackTask::Execute()
    {
        auto *device = RHI::Get()->GetDevice();
        auto *queue = device->GetQueue(rhi::QueueType::GRAPHICS);
        device->WaitIdle();

        auto id = queue->ReadImage(image, [this](const uint8_t* ptr, uint32_t slice, uint64_t size) {
            auto buildCellPos = context->sortedCells[cellIndex];
            uint8_t* cell = context->GetOrAllocateCellData(buildCellPos);

            const uint32_t* data = reinterpret_cast<const uint32_t*>(ptr);
            uint32_t num = static_cast<uint32_t>(size / sizeof(uint32_t));

            for (uint32_t i = 0; i < num; i++) {
                auto visibleID = data[i];
                if (visibleID == ~(0U)) {
                    continue; // not visible
                }

                uint32_t byteInData = visibleID / 8;
                uint32_t bitInByte = visibleID % 8;

                cell[byteInData] |= (1U << bitInByte);
            }
        });

        queue->Wait(id);
        auto buildCellPos = context->sortedCells[cellIndex];
        uint8_t* cell = context->GetOrAllocateCellData(buildCellPos);
#if 0
        printf("CellID %u CellVisibleNum %u: ", cellIndex, CountBits(cell, context->cellDataSize));
        for (uint32_t i = 0; i < context->cellDataSize; i++) {
            printf("%u ", static_cast<uint32_t>(cell[i]));
        }
        printf("\n");
#endif
    }


    void PVSBuildContext::Save()
    {
        FilePath pvsHeaderPath = savePath / FilePath(PVSSectorProvider::HeaderFileName());
        const auto &workFs = AssetDataBase::Get()->GetWorkSpaceFs();
        auto pvsHeaderFile = workFs->CreateOrOpenFile(pvsHeaderPath);
        auto archive = pvsHeaderFile->WriteAsArchive();
        BinaryOutputArchive bin(*archive);
        config.Save(bin);

        for (auto& [coord, sector] : sectors) {
            FilePath pvsSectorData = savePath / FilePath(PVSSectorProvider::SectorFileName(coord));
            auto pvsSectorFile = workFs->CreateOrOpenFile(pvsSectorData);
            auto sectorArchive = pvsSectorFile->WriteAsArchive();

            BinaryOutputArchive sectorBin(*sectorArchive);
            sector.sector->Save(sectorBin);
        }
    }
} // namespace sky::editor
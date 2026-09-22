//
// Created by Zach Lee on 2026/2/23.
//

#pragma once

#include <pvs/PVSLoader.h>
#include <pvs/PVSVisibility.h>
#include <pvs/PVSVisualizer.h>
#include <render/RenderScene.h>

namespace sky {

    struct PVSCullingViewData : RenderSceneCullingViewData {
        const uint8_t *data = nullptr;
        uint32_t       dataSizeInBytes = 0;
    };

    /**
     * @brief Manages PVS-based visibility culling for a render scene
     */
    class PVSCulling : public IRenderSceneCulling {
    public:
        explicit PVSCulling();
        ~PVSCulling() override = default;

        bool Init(const FilePath& path);

        /**
         * @brief Clear all PVS data and reset the system
         */
        void Clear();

        void SetVisualizer(PVSVisualizer* inVisualizer) noexcept { visualizer.reset(inVisualizer); }
        PVSVisualizer* GetVisualizer() const noexcept { return visualizer.get(); }
    private:
        bool IsActive() const noexcept override { return !!loader; }

        void UpdateByMainView(const Vector3& pos) noexcept override;

        RenderSceneCullingViewData* PrepareCullingViewData(const SceneView* view) const noexcept override;

        bool QueryVisible(const RenderSceneCullingViewData* data, uint32_t id) const noexcept override;

        std::vector<PVSVisibilityViewID> visibilityData; // per primitive

        std::unique_ptr<PVSLoader> loader;

        std::unique_ptr<PVSVisualizer> visualizer;
    };

} // namespace sky
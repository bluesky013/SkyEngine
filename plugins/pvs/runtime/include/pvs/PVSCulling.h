//
// Created by Zach Lee on 2026/2/23.
//

#pragma once

#include <pvs/PVSLoader.h>
#include <pvs/PVSVisualizer.h>
#include <render/RenderScene.h>

namespace sky {

    using PVSObjectID = uint32_t;
    static constexpr PVSObjectID INVALID_PVS_OBJECT = 0xFFFFFF00;

    #define PVS_OBJECT_MASK_IN_BYTES_BIT 8
    #define PVS_OBJECT_INDEX_IN_BYTES_BIT 24
    static_assert((PVS_OBJECT_MASK_IN_BYTES_BIT + PVS_OBJECT_INDEX_IN_BYTES_BIT) == sizeof(PVSObjectID) * 8);

    static constexpr PVSObjectID MAX_OBJECTS = (1 << 24) - 2; // 0xFFFFFF invalid object id.

    /**
     * @brief Unique identifier for bitset visit
     */
    struct PVSVisibilityViewID {
        FORCEINLINE bool IsValid() const { return (value & INVALID_PVS_OBJECT) != INVALID_PVS_OBJECT; }

        explicit PVSVisibilityViewID() : value{INVALID_PVS_OBJECT} {}

        explicit PVSVisibilityViewID(PVSObjectID objectID) : value{objectID} {}

        union {
            PVSObjectID value;
            struct {
                uint32_t maskInBytes  : PVS_OBJECT_MASK_IN_BYTES_BIT;
                uint32_t indexInBytes : PVS_OBJECT_INDEX_IN_BYTES_BIT;
            };
        };
    };

    struct PVSCullingViewData : RenderSceneCullingViewData {
        const uint8_t* data = nullptr;
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
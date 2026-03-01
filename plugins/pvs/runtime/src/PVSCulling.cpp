//
// Created by Zach Lee on 2026/2/23.
//

#include <pvs/PVSCulling.h>

namespace sky {

    PVSCulling::PVSCulling()
    {
    }

    bool PVSCulling::Init(const FilePath& path)
    {
        std::unique_ptr<PVSSectorProvider> provider = std::make_unique<PVSSectorProvider>(path);\

        PVSConfig config = {};
        if (provider->LoadHeader(config)) {
            loader = std::make_unique<PVSLoader>(config);
            loader->SetProvider(provider.release());
            return true;
        }
        return false;
    }

    void PVSCulling::Clear()
    {
        loader = nullptr;
    }

    void PVSCulling::UpdateByMainView(const Vector3& pos) noexcept
    {
        if (loader) {
            loader->Update(pos);
        }
    }

    RenderSceneCullingViewData* PVSCulling::PrepareCullingViewData(const SceneView* view) const noexcept
    {
        const auto& config = loader->GetConfig();
        const uint8_t* data = loader->QueryVisibility(config.CalculateCellCoordByWorldPosition(view->GetViewOrigin()));

        PVSCullingViewData* cullData = nullptr;
        if (data != nullptr) {
            cullData = new PVSCullingViewData();
            cullData->data = data;
        }

        return cullData;
    }

    bool PVSCulling::QueryVisible(const RenderSceneCullingViewData* data, uint32_t id) const noexcept
    {
        const PVSCullingViewData* pvsData = reinterpret_cast<const PVSCullingViewData*>(data);

        uint32_t byteIndex = id / 8;
        uint32_t bitIndex = id % 8;
        // PVSVisibilityViewID objID(id);
        // return (pvsData->data[objID.indexInBytes] & objID.maskInBytes) == objID.maskInBytes;
        return pvsData->data[byteIndex] & (1 << bitIndex);
    }

} // namespace sky
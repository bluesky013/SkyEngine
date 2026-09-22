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
        auto provider = std::make_unique<PVSSectorProvider>(path);

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
        if (loader == nullptr || view == nullptr) {
            return nullptr;
        }

        const auto& config = loader->GetConfig();
        const uint8_t* data = loader->QueryVisibility(config.CalculateCellCoordByWorldPosition(view->GetViewOrigin()));
        if (data == nullptr) {
            return nullptr;
        }

        auto* cullData = new PVSCullingViewData();
        cullData->data = data;
        cullData->dataSizeInBytes = loader->GetCellDataSize();
        return cullData;
    }

    bool PVSCulling::QueryVisible(const RenderSceneCullingViewData* data, uint32_t id) const noexcept
    {
        const auto* pvsData = reinterpret_cast<const PVSCullingViewData*>(data);
        if (pvsData == nullptr) {
            return true;
        }
        return QueryPVSObjectVisible(pvsData->data, pvsData->dataSizeInBytes, id);
    }

} // namespace sky

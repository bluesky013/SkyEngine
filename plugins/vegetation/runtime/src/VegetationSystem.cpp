//
// Created on 2026/09/22.
//

#include <vegetation/VegetationSystem.h>

#include <algorithm>
#include <cmath>

namespace sky::vegetation {

    namespace {

        inline uint64_t MakeCellKey(int32_t cellX, int32_t cellY)
        {
            return (static_cast<uint64_t>(static_cast<uint32_t>(cellX)) << 32) |
                   static_cast<uint64_t>(static_cast<uint32_t>(cellY));
        }

        inline Vector3 CellCenter(float cellSize, int32_t cellX, int32_t cellY)
        {
            return Vector3(
                (static_cast<float>(cellX) + 0.5f) * cellSize,
                0.f,
                (static_cast<float>(cellY) + 0.5f) * cellSize);
        }

    } // namespace

    bool VegetationCellTask::DoWork()
    {
        if (hasInstances) {
            cell.instances = instances;
            finished.store(true);
            return true;
        }

        if (provider == nullptr || config == nullptr || palette == nullptr) {
            return false;
        }
        GenerateCellInstances(*provider, *config, *palette, cell.cellX, cell.cellY, densityScale, cell.instances);
        finished.store(true);
        return true;
    }

    VegetationSystem::VegetationSystem()
    {
        renderAdaptor.reset(VegetationRenderFactory::Get()->CreateAdaptor());
    }

    VegetationSystem::~VegetationSystem()
    {
        if (surfaceProvider != nullptr) {
            surfaceProvider->RemoveSurfaceListener(this);
        }
        CancelPending();
    }

    void VegetationSystem::SetSurfaceProvider(IVegetationSurfaceProvider *provider)
    {
        if (surfaceProvider == provider) {
            return;
        }
        if (surfaceProvider != nullptr) {
            surfaceProvider->RemoveSurfaceListener(this);
        }
        surfaceProvider = provider;
        if (surfaceProvider != nullptr) {
            surfaceProvider->AddSurfaceListener(this);
        }
        RebuildInstancesByCell();
    }

    void VegetationSystem::SetInstances(const std::vector<VegetationInstance> &inInstances)
    {
        instances = inInstances;
        RebuildInstancesByCell();
    }

    void VegetationSystem::RebuildInstancesByCell()
    {
        instancesByCell.clear();
        if (surfaceProvider == nullptr) {
            return;
        }

        const float cellSize = surfaceProvider->GetCellSize();
        if (cellSize <= 0.f) {
            return;
        }

        for (const auto &instance : instances) {
            const int32_t cellX = static_cast<int32_t>(std::floor(instance.position.x / cellSize));
            const int32_t cellY = static_cast<int32_t>(std::floor(instance.position.z / cellSize));
            instancesByCell[MakeCellKey(cellX, cellY)].push_back(instance);
        }
    }

    void VegetationSystem::OnVegetationSurfaceChanged(const AABB &region)
    {
        NotifyRegionChanged(region);
    }

    void VegetationSystem::CancelPending()
    {
        for (auto &entry : pendingCells) {
            entry.second->ResetTask();
        }
        pendingCells.clear();
    }

    void VegetationSystem::OnDetachFromWorld(World &world)
    {
        CancelPending();
        loadedCells.clear();
    }

    bool VegetationSystem::SampleSurface(const Vector3 &worldPos, VegetationSurfaceSample &out) const
    {
        if (surfaceProvider == nullptr) {
            out = VegetationSurfaceSample{};
            return false;
        }
        return surfaceProvider->SampleSurface(worldPos, out);
    }

    bool VegetationSystem::IsCellLoaded(int32_t cellX, int32_t cellY) const
    {
        return loadedCells.count(MakeCellKey(cellX, cellY)) != 0;
    }

    float VegetationSystem::DensityScaleAt(float distanceSq) const
    {
        const float near = loadRadius * 0.5f;
        const float d    = std::sqrt(std::max(distanceSq, 0.f));
        if (d <= near) {
            return 1.f;
        }
        const float span = std::max(loadRadius - near, 1e-3f);
        const float t    = std::min((d - near) / span, 1.f);
        return 1.f - t * 0.9f;
    }

    void VegetationSystem::NotifyRegionChanged(const AABB &region)
    {
        if (surfaceProvider == nullptr) {
            return;
        }

        int32_t minX = 0;
        int32_t minY = 0;
        int32_t maxX = 0;
        int32_t maxY = 0;
        surfaceProvider->GetCellRange(region, minX, minY, maxX, maxY);

        for (int32_t y = minY; y <= maxY; ++y) {
            for (int32_t x = minX; x <= maxX; ++x) {
                const uint64_t key = MakeCellKey(x, y);
                if (loadedCells.erase(key) > 0 && renderAdaptor) {
                    renderAdaptor->OnCellUnloaded(x, y);
                }
                const auto iter = pendingCells.find(key);
                if (iter != pendingCells.end()) {
                    iter->second->ResetTask();
                    pendingCells.erase(iter);
                }
            }
        }
    }

    void VegetationSystem::Update()
    {
        if (!streamingEnabled || surfaceProvider == nullptr || !hasPalette) {
            return;
        }

        const float cellSize = surfaceProvider->GetCellSize();
        if (cellSize <= 0.f) {
            return;
        }

        const float unloadSq = unloadRadius * unloadRadius;
        const float loadSq   = loadRadius * loadRadius;

        // Unload cells beyond the unload radius.
        for (auto iter = loadedCells.begin(); iter != loadedCells.end();) {
            const auto &cell = iter->second;
            const Vector3 center = CellCenter(cellSize, cell.cellX, cell.cellY);
            const float dx = center.x - focus.x;
            const float dz = center.z - focus.z;
            if (dx * dx + dz * dz > unloadSq) {
                if (renderAdaptor) {
                    renderAdaptor->OnCellUnloaded(cell.cellX, cell.cellY);
                }
                iter = loadedCells.erase(iter);
            } else {
                ++iter;
            }
        }

        for (auto iter = pendingCells.begin(); iter != pendingCells.end();) {
            const auto &task = iter->second;
            const Vector3 center = CellCenter(cellSize, task->GetCell().cellX, task->GetCell().cellY);
            const float dx = center.x - focus.x;
            const float dz = center.z - focus.z;
            if (dx * dx + dz * dz > unloadSq) {
                task->ResetTask();
                iter = pendingCells.erase(iter);
            } else {
                ++iter;
            }
        }

        const int32_t focusX = static_cast<int32_t>(std::floor(focus.x / cellSize));
        const int32_t focusY = static_cast<int32_t>(std::floor(focus.z / cellSize));
        const int32_t radius = static_cast<int32_t>(std::ceil(loadRadius / cellSize));

        // Start prefetch for cells within the load radius.
        for (int32_t y = focusY - radius; y <= focusY + radius; ++y) {
            for (int32_t x = focusX - radius; x <= focusX + radius; ++x) {
                const uint64_t key = MakeCellKey(x, y);
                if (loadedCells.count(key) != 0 || pendingCells.count(key) != 0) {
                    continue;
                }

                const Vector3 center = CellCenter(cellSize, x, y);
                const float dx = center.x - focus.x;
                const float dz = center.z - focus.z;
                const float distanceSq = dx * dx + dz * dz;
                if (distanceSq > loadSq) {
                    continue;
                }

                auto task = CounterPtr<VegetationCellTask>(new VegetationCellTask());
                const auto instancesIt = instancesByCell.find(key);
                const std::vector<VegetationInstance> *instances = instancesIt != instancesByCell.end() ? &instancesIt->second : nullptr;
                task->Setup(surfaceProvider, &config, &palette, x, y, DensityScaleAt(distanceSq), instances);
                task->StartAsync();
                pendingCells.emplace(key, task);
            }
        }

        // Apply finished cells within the per-tick budget.
        uint32_t applied = 0;
        for (auto iter = pendingCells.begin(); iter != pendingCells.end() && applied < loadBudget;) {
            if (!iter->second->IsFinished()) {
                ++iter;
                continue;
            }

            const VegetationCell cell = iter->second->GetCell();
            loadedCells.emplace(iter->first, cell);
            if (renderAdaptor) {
                renderAdaptor->OnCellLoaded(VegetationRenderCell{cell.cellX, cell.cellY, cell.instances});
            }
            iter = pendingCells.erase(iter);
            ++applied;
        }
    }

} // namespace sky::vegetation

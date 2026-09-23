//
// Created on 2026/09/22.
//

#pragma once

#include <framework/world/World.h>

#include <vegetation/VegetationPlacement.h>
#include <vegetation/VegetationRenderAdaptor.h>
#include <vegetation/VegetationSurface.h>
#include <vegetation/VegetationSystemInterface.h>
#include <vegetation/VegetationTypes.h>

#include <core/async/Task.h>

#include <atomic>
#include <unordered_map>
#include <vector>

namespace sky::vegetation {

    struct VegetationCell {
        int32_t                         cellX = 0;
        int32_t                         cellY = 0;
        std::vector<VegetationInstance> instances;
    };

    // Off-thread cell population (deterministic placement).
    class VegetationCellTask : public Task {
    public:
        void Setup(const IVegetationSurfaceProvider *inProvider, const VegetationPlacementConfig *inConfig,
                   const VegetationPalette *inPalette, int32_t inCellX, int32_t inCellY, float inDensityScale,
                   const std::vector<VegetationInstance> *inInstances = nullptr)
        {
            provider     = inProvider;
            config       = inConfig;
            palette      = inPalette;
            densityScale = inDensityScale;
            cell.cellX   = inCellX;
            cell.cellY   = inCellY;
            if (inInstances != nullptr) {
                instances    = *inInstances;
                hasInstances = true;
            }
        }

        const VegetationCell &GetCell() const { return cell; }
        bool                  IsFinished() const { return finished.load(); }

    protected:
        bool DoWork() override;

    private:
        const IVegetationSurfaceProvider *provider     = nullptr;
        const VegetationPlacementConfig  *config       = nullptr;
        const VegetationPalette          *palette      = nullptr;
        float                             densityScale = 1.f;
        std::vector<VegetationInstance>   instances;
        bool                              hasInstances  = false;
        VegetationCell                    cell;
        std::atomic_bool                  finished{false};
    };

    // Per-world vegetation runtime: owns the palette/config and pages cells around a focus position.
    class VegetationSystem : public IWorldSubSystem, public IVegetationSurfaceListener, public IVegetationSystem {
    public:
        VegetationSystem();
        ~VegetationSystem() override;

        static constexpr std::string_view NAME = "Vegetation";

        void SetSurfaceProvider(IVegetationSurfaceProvider *provider) override;
        const IVegetationSurfaceProvider *GetSurfaceProvider() const { return surfaceProvider; }
        bool HasSurface() const override { return surfaceProvider != nullptr; }

        // IVegetationSurfaceListener: invalidate cells overlapping a changed surface region.
        void OnVegetationSurfaceChanged(const AABB &region) override;

        void SetPalette(const VegetationPalette &inPalette) override
        {
            palette    = inPalette;
            hasPalette = true;
        }
        void SetPlacementConfig(const VegetationPlacementConfig &inConfig) override { config = inConfig; }

        // Asset-provided instances (e.g. imported foliage) override procedural placement for their cells.
        void SetInstances(const std::vector<VegetationInstance> &inInstances);

        void SetStreamingEnabled(bool enable) override { streamingEnabled = enable; }
        void SetStreamingFocus(const Vector3 &position) override { focus = position; }
        void SetStreamingRadii(float load, float unload) override
        {
            loadRadius   = load;
            unloadRadius = unload;
        }
        void SetLoadBudget(uint32_t budget) override { loadBudget = budget; }

        // Returns false (and leaves out invalid) when no surface provider is registered.
        bool SampleSurface(const Vector3 &worldPos, VegetationSurfaceSample &out) const override;

        void Update();
        void Tick(float time) override { Update(); }

        uint32_t GetLoadedCellCount() const override { return static_cast<uint32_t>(loadedCells.size()); }
        uint32_t GetPendingCellCount() const { return static_cast<uint32_t>(pendingCells.size()); }
        const std::unordered_map<uint64_t, VegetationCell> &GetLoadedCells() const { return loadedCells; }
        bool IsCellLoaded(int32_t cellX, int32_t cellY) const override;

        // Invalidates cells overlapping a changed surface region.
        void NotifyRegionChanged(const AABB &region) override;

    private:
        void OnAttachToWorld(World &world) override {}
        void OnDetachFromWorld(World &world) override;

        void  CancelPending();
        void  RebuildInstancesByCell();
        float DensityScaleAt(float distanceSq) const;

        IVegetationSurfaceProvider *surfaceProvider = nullptr;
        std::unique_ptr<IVegetationRenderAdaptor> renderAdaptor;
        VegetationPalette           palette;
        VegetationPlacementConfig   config;
        bool                        hasPalette = false;

        std::vector<VegetationInstance> instances;
        std::unordered_map<uint64_t, std::vector<VegetationInstance>> instancesByCell;

        bool     streamingEnabled = false;
        Vector3  focus;
        float    loadRadius   = 64.f;
        float    unloadRadius = 96.f;
        uint32_t loadBudget   = 8;

        std::unordered_map<uint64_t, VegetationCell>                  loadedCells;
        std::unordered_map<uint64_t, CounterPtr<VegetationCellTask>>  pendingCells;
    };

} // namespace sky::vegetation

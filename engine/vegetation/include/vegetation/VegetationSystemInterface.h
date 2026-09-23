//
// Created on 2026/09/22.
//

#pragma once

#include <vegetation/VegetationSurface.h>
#include <vegetation/VegetationTypes.h>

#include <core/math/Vector3.h>
#include <core/shapes/AABB.h>

#include <cstdint>

namespace sky::vegetation {

    // Engine-side vegetation sub-system interface (implementation lives in a plugin).
    class IVegetationSystem {
    public:
        virtual ~IVegetationSystem() = default;

        virtual void SetSurfaceProvider(IVegetationSurfaceProvider *provider) = 0;
        virtual bool HasSurface() const = 0;

        virtual void SetPalette(const VegetationPalette &palette) = 0;
        virtual void SetPlacementConfig(const VegetationPlacementConfig &config) = 0;

        virtual void SetStreamingEnabled(bool enable) = 0;
        virtual void SetStreamingFocus(const Vector3 &position) = 0;
        virtual void SetStreamingRadii(float load, float unload) = 0;
        virtual void SetLoadBudget(uint32_t budget) = 0;

        virtual bool SampleSurface(const Vector3 &worldPos, VegetationSurfaceSample &out) const = 0;

        virtual uint32_t GetLoadedCellCount() const = 0;
        virtual bool     IsCellLoaded(int32_t cellX, int32_t cellY) const = 0;
        virtual void     NotifyRegionChanged(const AABB &region) = 0;
    };

} // namespace sky::vegetation

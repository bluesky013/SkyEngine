//
// Created on 2026/09/23.
//

#pragma once

#include <vegetation/VegetationTypes.h>

#include <core/environment/Singleton.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace sky::vegetation {

    // Plain-data handoff for one loaded cell (no render types).
    struct VegetationRenderCell {
        int32_t                         cellX = 0;
        int32_t                         cellY = 0;
        std::vector<VegetationInstance> instances;
    };

    // Render adaptor seam: implemented by the render layer (e.g. aurora). The core pushes plain data.
    class IVegetationRenderAdaptor {
    public:
        virtual ~IVegetationRenderAdaptor() = default;

        virtual void OnCellLoaded(const VegetationRenderCell &cell) = 0;
        virtual void OnCellUnloaded(int32_t cellX, int32_t cellY) = 0;
    };

    // Backend/render factory seam (no render types), mirroring the physics/navigation factories.
    class VegetationRenderFactory : public Singleton<VegetationRenderFactory> {
    public:
        class Impl {
        public:
            Impl()          = default;
            virtual ~Impl() = default;

            virtual IVegetationRenderAdaptor *CreateAdaptor() = 0;
        };

        void Register(Impl *impl);
        void UnRegister();
        IVegetationRenderAdaptor *CreateAdaptor();

    private:
        std::unique_ptr<Impl> factory;
    };

} // namespace sky::vegetation

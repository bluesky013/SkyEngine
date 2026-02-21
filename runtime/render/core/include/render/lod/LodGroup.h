//
// Created by blues on 2026/2/16.
//

#pragma once

#include <render/RenderResource.h>
#include <render/RenderGeometry.h>
#include <render/lod/LodTypes.h>
#include <core/math/Vector4.h>
#include <core/math/Matrix4.h>
#include <core/shapes/AABB.h>
#include <vector>
#include <memory>

namespace sky {

    class LodGroup : public RenderResource {
    public:
        LodGroup() = default;
        ~LodGroup() override = default;

        void Init(size_t lodNum) noexcept;

        void AddLod(LodProxy* proxy, uint32_t lodIndex) noexcept;

        uint32_t SelectLod(const Vector3& origin, float sphereRadius, const Vector3& viewOrigin, const Matrix4& proj) const noexcept;

        uint32_t GetMaxSectionNumWithSkin() const noexcept;

        FORCEINLINE const LodProxy* GetLod(uint32_t lodIndex) const { return lodProxys[lodIndex].get(); }
        FORCEINLINE uint32_t GetLodCount() const { return static_cast<uint32_t>(lodProxys.size()); }
    private:
        LodConfig config;

        std::vector<std::unique_ptr<LodProxy>> lodProxys;
    };

    using RDLodGroupPtr = CounterPtr<LodGroup>;

} // namespace sky

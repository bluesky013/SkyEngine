//
// Created by blues on 2026/2/16.
//

#pragma once

#include <core/platform/Platform.h>
#include <render/resource/Mesh.h>

namespace sky {

    struct LodConfig {
        uint32_t lodBias = 0;
        float scaleFactor = 1.0f;
    };

    struct LodLevel {
        float screenSize = 1.f;
    };

    class LodProxy {
    public:
        explicit LodProxy(const LodLevel &inScreenSize) : level(inScreenSize) {}
        virtual ~LodProxy() = default;

        virtual bool IsValid() const noexcept = 0;

        virtual void Reset() noexcept {}

        virtual uint32_t GetSectionNum() const noexcept { return 1; }

        virtual bool HasSkin() const noexcept { return false; }

        virtual RDMeshPtr GetMesh() const { return {}; }

        void SetLocalBounds(const BoundingBoxSphere &bounds)
        {
            localBounds = bounds;
        }

        FORCEINLINE const LodLevel &GetLevel() const { return level; }

        FORCEINLINE const BoundingBoxSphere &GetLocalBounds() const
        {
            return localBounds;
        }
    protected:
        LodLevel level;

        BoundingBoxSphere localBounds;
    };

} // namespace sky

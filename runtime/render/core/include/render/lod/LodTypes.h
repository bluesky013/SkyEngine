//
// Created by blues on 2026/2/16.
//

#pragma once

#include <core/platform/Platform.h>
#include <render/RenderGeometry.h>
#include <render/RenderDrawArgs.h>

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

        virtual bool IsValid() const = 0;

        virtual void Reset() {}

        virtual void FillVertexFlagAndArgs(std::vector<DrawArgs>& args) {}

        FORCEINLINE const LodLevel &GetLevel() const { return level; }

    protected:
        LodLevel level;
    };

} // namespace sky

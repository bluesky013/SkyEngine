//
// Created by blues on 2024/10/11.
//

#pragma once

#include <render/debug/DebugRenderer.h>
#include <DebugDraw.h>

namespace sky::ai {

    class RecastDebugDraw : public duDebugDraw {
    public:
        RecastDebugDraw() = default;
        ~RecastDebugDraw() override = default;

        void depthMask(bool state) override;
        void texture(bool state) override;
        void begin(duDebugDrawPrimitives prim, float size = 1.0f) override;
        void vertex(const float* pos, unsigned int color) override;
        void vertex(const float x, const float y, const float z, unsigned int color) override;
        void vertex(const float* pos, unsigned int color, const float* uv) override;
        void vertex(const float x, const float y, const float z, unsigned int color, const float u, const float v) override;
        void end() override;

    private:
        std::unique_ptr<DebugRenderer> dr;
    };

} // namespace sky

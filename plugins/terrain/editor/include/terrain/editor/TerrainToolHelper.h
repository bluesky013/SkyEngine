//
// Created by blues on 2024/11/30.
//

#pragma once

#include <render/debug/DebugRenderer.h>
#include <render/RenderPrimitive.h>
#include <terrain/TerrainComponent.h>
#include <memory>

namespace sky::editor {

    class TerrainHelper {
    public:
        TerrainHelper();
        ~TerrainHelper();

        void Reset();
        void DrawFullTerrainGrid(const TerrainBuildConfig &cfg, const Vector3 &worldPos);
        void DrawSelectedGrid(const TerrainData &data, int32_t x, int32_t y, const Vector3 &worldPos);
        void DrawTerrainBound(const TerrainData &data, const Vector3 &worldPos);

        const std::vector<RenderPrimitive*> &GetPrimitives()const { return primitives; }
    private:
        CounterPtr<Technique> technique;
        std::vector<RenderPrimitive*> primitives;
        std::unique_ptr<DebugRenderer> renderer;
    };

} // namespace sky::editor

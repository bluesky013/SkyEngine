//
// Created by Zach Lee on 2023/9/2.
//

#pragma once

#include <core/math/Color.h>
#include <core/math/Vector3.h>
#include <core/shapes/Shapes.h>
#include <render/RenderPrimitive.h>
#include <render/resource/Technique.h>

namespace sky {

    struct GeometryBatchVertex {
        Vector3 position;
        Vector3 normal;
        Vector3 color;
    };

    struct GeometryDrawBatch {
        std::vector<GeometryBatchVertex> batchVertices;
        std::unique_ptr<RenderPrimitive> primitive;
        DrawArgs args;
    };

    class GeometryRenderer {
    public:
        GeometryRenderer() = default;
        ~GeometryRenderer() = default;

        // customer technique
        GeometryRenderer &SetTechnique(const RDGfxTechPtr &tech);

        GeometryRenderer &SetColor(const Color &color);

        GeometryRenderer &DrawLine(const Line &);
        GeometryRenderer &DrawTriangle(const Triangle &triangle);
        GeometryRenderer &DrawQuad(const Quad &quad);
    private:
        void AddVertex(const GeometryBatchVertex &vtx);

        RDGfxTechPtr technique;

        Color currentColor = Color{1.f, 1.f, 1.f, 1.f};
        rhi::PipelineState batchKey;

        GeometryDrawBatch *currentBatch = nullptr;
    };

} // namespace sky
//
// Created by Zach Lee on 2023/9/2.
//

#pragma once

#include <core/math/Color.h>
#include <core/math/Vector3.h>
#include <core/shapes/Shapes.h>
#include <render/RenderPrimitive.h>
#include <render/resource/Technique.h>
#include <render/resource/ResourceGroup.h>

namespace sky {

    struct GeometryBatchVertex {
        Vector4 position;
        Color color;
    };

    class GeometryRenderer {
    public:
        GeometryRenderer() = default;
        ~GeometryRenderer() = default;

        void Init();
        void Upload();
        void UpdateTransform(const Matrix4 &matrix);

        // customer technique
        GeometryRenderer &SetTechnique(const RDGfxTechPtr &tech);
        GeometryRenderer &SetColor(const Color &color);
        GeometryRenderer &DrawLine(const Line &);
        GeometryRenderer &DrawTriangle(const Triangle &triangle);
        GeometryRenderer &DrawQuad(const Quad &quad);
        GeometryRenderer &Clear();

        RenderPrimitive *GetPrimitive() const { return primitive.get(); }

    private:
        void ResetPrimitive(const RDGfxTechPtr &tech);
        void ResizeVertex(uint32_t size);
        void AddVertex(const GeometryBatchVertex &vtx);

        RDGfxTechPtr technique;

        Color currentColor = Color{1.f, 1.f, 1.f, 1.f};

        uint32_t currentVertex = 0;
        uint32_t vertexCapacity = 0;
        std::vector<GeometryBatchVertex> batchVertices;
        std::unique_ptr<RenderPrimitive> primitive;
        RDBufferPtr vertexBuffer;
        RDDynamicUniformBufferPtr ubo;

        rhi::CmdDrawLinear args;
    };

} // namespace sky

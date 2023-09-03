//
// Created by Zach Lee on 2023/9/2.
//

#include <render/geometry/GeometryRenderer.h>

namespace sky {

    GeometryRenderer &GeometryRenderer::SetTechnique(const RDGfxTechPtr &tech)
    {
        technique = tech;
        return *this;
    }

    GeometryRenderer &GeometryRenderer::SetColor(const Color &color)
    {
        return *this;
    }

    GeometryRenderer &GeometryRenderer::DrawLine(const Line &)
    {
        return *this;
    }

    GeometryRenderer &GeometryRenderer::DrawTriangle(const Triangle &triangle)
    {
        return *this;
    }

    GeometryRenderer &GeometryRenderer::DrawQuad(const Quad &quad)
    {
        return *this;
    }

    void GeometryRenderer::AddVertex(const GeometryBatchVertex &vtx)
    {
    }

} // namespace sky
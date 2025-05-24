//
// Created by blues on 2025/5/5.
//

#include <render/geometry/GeometryFactory.h>

namespace sky {

    VFPos3F_UV2F CUBE_VTX_DATA[] = {
        // (Front face)
        { { -0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f } },
        { {  0.5f, -0.5f,  0.5f }, { 1.0f, 0.0f } },
        { {  0.5f,  0.5f,  0.5f }, { 1.0f, 1.0f } },
        { { -0.5f,  0.5f,  0.5f }, { 0.0f, 1.0f } },

        // (Back face)
        { {  0.5f, -0.5f, -0.5f },  { 0.0f, 0.0f } },
        { { -0.5f, -0.5f, -0.5f },  { 1.0f, 0.0f } },
        { { -0.5f,  0.5f, -0.5f },  { 1.0f, 1.0f } },
        { {  0.5f,  0.5f, -0.5f },  { 0.0f, 1.0f } },

        // (Top face)
        { { -0.5f,  0.5f,  0.5f }, { 0.0f, 0.0f } },
        { {  0.5f,  0.5f,  0.5f }, { 1.0f, 0.0f } },
        { {  0.5f,  0.5f, -0.5f }, { 1.0f, 1.0f } },
        { { -0.5f,  0.5f, -0.5f }, { 0.0f, 1.0f } },

        // (Bottom face)
        { { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f } },
        { {  0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f } },
        { {  0.5f, -0.5f,  0.5f }, { 1.0f, 1.0f } },
        { { -0.5f, -0.5f,  0.5f }, { 0.0f, 1.0f } },

        // (Right face)
        { {  0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f } },
        { {  0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f } },
        { {  0.5f,  0.5f, -0.5f }, { 1.0f, 1.0f } },
        { {  0.5f,  0.5f,  0.5f }, { 0.0f, 1.0f } },

        // (Left face)
        { { -0.5f, -0.5f, -0.5f },  { 0.0f, 0.0f } },
        { { -0.5f, -0.5f,  0.5f },  { 1.0f, 0.0f } },
        { { -0.5f,  0.5f,  0.5f },  { 1.0f, 1.0f } },
        { { -0.5f,  0.5f, -0.5f },  { 0.0f, 1.0f } }
    };

    VFShading_NT CUBE_SHADING_DATA[] = {
        {{ 0.0f,  0.0f,  1.0f, 1.0}, { 1.0f,   0.0f,  0.0f,  1.0f } },
        {{ 0.0f,  0.0f,  1.0f, 1.0}, { 1.0f,   0.0f,  0.0f,  1.0f } },
        {{ 0.0f,  0.0f,  1.0f, 1.0}, { 1.0f,   0.0f,  0.0f,  1.0f } },
        {{ 0.0f,  0.0f,  1.0f, 1.0}, { 1.0f,   0.0f,  0.0f,  1.0f } },
        {{ 0.0f,  0.0f, -1.0f, 1.0}, { -1.0f,  0.0f,  0.0f,  1.0f }},
        {{ 0.0f,  0.0f, -1.0f, 1.0}, { -1.0f,  0.0f,  0.0f,  1.0f }},
        {{ 0.0f,  0.0f, -1.0f, 1.0}, { -1.0f,  0.0f,  0.0f,  1.0f }},
        {{ 0.0f,  0.0f, -1.0f, 1.0}, { -1.0f,  0.0f,  0.0f,  1.0f }},
        {{ 0.0f,  1.0f,  0.0f, 1.0}, { 1.0f,   0.0f,  0.0f,  1.0f } },
        {{ 0.0f,  1.0f,  0.0f, 1.0}, { 1.0f,   0.0f,  0.0f,  1.0f } },
        {{ 0.0f,  1.0f,  0.0f, 1.0}, { 1.0f,   0.0f,  0.0f,  1.0f } },
        {{ 0.0f,  1.0f,  0.0f, 1.0}, { 1.0f,   0.0f,  0.0f,  1.0f } },
        {{ 0.0f, -1.0f,  0.0f, 1.0}, { 1.0f,   0.0f,  0.0f,  1.0f } },
        {{ 0.0f, -1.0f,  0.0f, 1.0}, { 1.0f,   0.0f,  0.0f,  1.0f } },
        {{ 0.0f, -1.0f,  0.0f, 1.0}, { 1.0f,   0.0f,  0.0f,  1.0f } },
        {{ 0.0f, -1.0f,  0.0f, 1.0}, { 1.0f,   0.0f,  0.0f,  1.0f } },
        {{ 1.0f,  0.0f,  0.0f, 1.0}, { 0.0f,   0.0f, -1.0f,  1.0f } },
        {{ 1.0f,  0.0f,  0.0f, 1.0}, { 0.0f,   0.0f, -1.0f,  1.0f } },
        {{ 1.0f,  0.0f,  0.0f, 1.0}, { 0.0f,   0.0f, -1.0f,  1.0f } },
        {{ 1.0f,  0.0f,  0.0f, 1.0}, { 0.0f,   0.0f, -1.0f,  1.0f } },
        {{-1.0f,  0.0f,  0.0f, 1.0}, { 0.0f,  0.0f,  1.0f,  1.0f } },
        {{-1.0f,  0.0f,  0.0f, 1.0}, { 0.0f,  0.0f,  1.0f,  1.0f } }
    };

    uint16_t CUBE_INDICES[] = {
        0, 1, 2, 0, 2, 3,
        4, 5, 6, 4, 6, 7,
        8, 9, 10, 8, 10, 11,
        12, 13, 14, 12, 14, 15,
        16, 17, 18, 16, 18, 19,
        20, 21, 22, 20, 22, 23
    };

    GeometryPtr CreateCube()
    {
        auto *geom = new TGeometry<VFPos3F_UV2F, VFShading_NT>();
        geom->SetVertexData(0, new VFRawDataStorage(reinterpret_cast<const uint8_t*>(CUBE_VTX_DATA), sizeof(CUBE_VTX_DATA)));
        geom->SetVertexData(1, new VFRawDataStorage(reinterpret_cast<const uint8_t*>(CUBE_SHADING_DATA), sizeof(CUBE_SHADING_DATA)));
        geom->SetIndices(new VFRawDataStorage(reinterpret_cast<const uint8_t*>(CUBE_INDICES), sizeof(CUBE_INDICES)), rhi::IndexType::U16);

        return {geom};
    }

    GeometryPtr GeometryFactory::CreateGeometry(BuiltinGeometryType type)
    {
        auto iter  = geometries.find(type);
        if (iter == geometries.end())
        {
            iter = geometries.emplace(type, CreateCube()).first;
        }

        return iter->second;
    }

} // namespace sky
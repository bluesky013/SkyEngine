//
// GeometryGenerator: parametric generation of builtin primitives (cube,
// plane, sphere, cylinder, cone, capsule). Pure math — no aurora/rhi types.
// Output is SoA (separate arrays per attribute), position as its own stream.
//

#pragma once

#include "core/math/Vector2.h"
#include "core/math/Vector3.h"
#include "core/math/Vector4.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace sky {

    struct GeometryStreams {
        std::vector<Vector3> positions;
        std::vector<Vector3> normals;
        std::vector<Vector4> tangents;   // xyz = tangent (UV U dir), w = handedness
        std::vector<Vector2> uvs;
        std::vector<uint32_t> indices;
    };

    GeometryStreams GenerateCube(float size);
    GeometryStreams GeneratePlane(float width, float depth, uint32_t segmentsX, uint32_t segmentsZ);
    GeometryStreams GenerateSphere(float radius, uint32_t rings, uint32_t sectors);
    GeometryStreams GenerateCylinder(float radiusTop, float radiusBottom, float height, uint32_t sectors);
    GeometryStreams GenerateCone(float radius, float height, uint32_t sectors);
    GeometryStreams GenerateCapsule(float radius, float height, uint32_t rings, uint32_t sectors);

    namespace geometry_detail {

        inline uint32_t EmitVertex(GeometryStreams &out, const Vector3 &p, const Vector3 &n, const Vector3 &t, float handedness, const Vector2 &uv)
        {
            out.positions.push_back(p);
            out.normals.push_back(n);
            out.tangents.emplace_back(t.x, t.y, t.z, handedness);
            out.uvs.push_back(uv);
            return static_cast<uint32_t>(out.positions.size() - 1);
        }

        inline void EmitTriangle(GeometryStreams &out, uint32_t a, uint32_t b, uint32_t c)
        {
            out.indices.push_back(a);
            out.indices.push_back(b);
            out.indices.push_back(c);
        }

        // Build a UV-aligned tangent + handedness from the U/V parameter
        // derivatives. Falls back to a reference axis when dPdU is degenerate.
        inline Vector4 MakeTangent(const Vector3 &normal, const Vector3 &dPdU, const Vector3 &dPdV)
        {
            Vector3 tangent = dPdU;
            const float len = tangent.Length();
            if (len < 1e-6f) {
                const Vector3 ref = (std::abs(normal.y) < 0.999f) ? VEC3_Y : VEC3_X;
                tangent = ref.Cross(normal);
                tangent.Normalize();
            } else {
                tangent = tangent / len;
            }

            Vector3 bitangent = dPdV;
            const float blen = bitangent.Length();
            if (blen >= 1e-6f) {
                bitangent = bitangent / blen;
            } else {
                bitangent = normal.Cross(tangent);
            }

            const float handedness = (normal.Cross(tangent).Dot(bitangent) < 0.0f) ? -1.0f : 1.0f;
            return Vector4(tangent.x, tangent.y, tangent.z, handedness);
        }

    } // namespace geometry_detail

    inline GeometryStreams GeneratePlane(float width, float depth, uint32_t segmentsX, uint32_t segmentsZ)
    {
        using namespace geometry_detail;
        GeometryStreams out;
        const float stepX = width / static_cast<float>(segmentsX);
        const float stepZ = depth / static_cast<float>(segmentsZ);
        const float halfW = width * 0.5f;
        const float halfD = depth * 0.5f;

        for (uint32_t i = 0; i <= segmentsZ; ++i) {
            const float v = static_cast<float>(i) / static_cast<float>(segmentsZ);
            const float z  = halfD - v * depth;
            for (uint32_t j = 0; j <= segmentsX; ++j) {
                const float u = static_cast<float>(j) / static_cast<float>(segmentsX);
                const float x = -halfW + u * width;
                EmitVertex(out, {x, 0.0f, z}, VEC3_Y, VEC3_X, 1.0f, {u, v});
            }
        }

        const uint32_t row = segmentsX + 1;
        for (uint32_t i = 0; i < segmentsZ; ++i) {
            for (uint32_t j = 0; j < segmentsX; ++j) {
                const uint32_t a = i * row + j;
                const uint32_t b = i * row + j + 1;
                const uint32_t c = (i + 1) * row + j + 1;
                const uint32_t d = (i + 1) * row + j;
                EmitTriangle(out, a, c, b);
                EmitTriangle(out, a, d, c);
            }
        }
        return out;
    }

    inline GeometryStreams GenerateCube(float size)
    {
        using namespace geometry_detail;
        GeometryStreams out;
        const float h = size * 0.5f;

        struct Face {
            Vector3 normal;
            Vector3 tangent;   // U axis
            Vector3 bitangent; // V axis
            Vector3 origin;    // corner at (u=0, v=0)
        };
        const Face faces[6] = {
            {  VEC3_X, -VEC3_Z,  VEC3_Y, { h, -h, -h}}, // +X
            { -VEC3_X,  VEC3_Z,  VEC3_Y, {-h, -h,  h}}, // -X
            {  VEC3_Y,  VEC3_X, -VEC3_Z, {-h,  h,  h}}, // +Y
            { -VEC3_Y,  VEC3_X,  VEC3_Z, {-h, -h, -h}}, // -Y
            {  VEC3_Z,  VEC3_X,  VEC3_Y, {-h, -h,  h}}, // +Z
            { -VEC3_Z, -VEC3_X,  VEC3_Y, { h, -h, -h}}, // -Z
        };

        for (const Face &f : faces) {
            const Vector3 c0 = f.origin;
            const Vector3 c1 = f.origin + f.tangent * size;
            const Vector3 c2 = f.origin + f.tangent * size + f.bitangent * size;
            const Vector3 c3 = f.origin + f.bitangent * size;

            const uint32_t a = EmitVertex(out, c0, f.normal, f.tangent, 1.0f, {0.0f, 0.0f});
            const uint32_t b = EmitVertex(out, c1, f.normal, f.tangent, 1.0f, {1.0f, 0.0f});
            const uint32_t c = EmitVertex(out, c2, f.normal, f.tangent, 1.0f, {1.0f, 1.0f});
            const uint32_t d = EmitVertex(out, c3, f.normal, f.tangent, 1.0f, {0.0f, 1.0f});

            EmitTriangle(out, a, b, c);
            EmitTriangle(out, a, c, d);
        }
        return out;
    }

    inline GeometryStreams GenerateSphere(float radius, uint32_t rings, uint32_t sectors)
    {
        using namespace geometry_detail;
        GeometryStreams out;

        for (uint32_t i = 0; i <= rings; ++i) {
            const float theta = PI * static_cast<float>(i) / static_cast<float>(rings); // 0..PI
            const float v     = static_cast<float>(i) / static_cast<float>(rings);
            const float sinT  = std::sin(theta);
            const float cosT  = std::cos(theta);
            for (uint32_t j = 0; j <= sectors; ++j) {
                const float phi  = 2.0f * PI * static_cast<float>(j) / static_cast<float>(sectors); // 0..2PI
                const float u    = static_cast<float>(j) / static_cast<float>(sectors);
                const float sinP = std::sin(phi);
                const float cosP = std::cos(phi);

                const Vector3 n = {sinT * cosP, cosT, sinT * sinP};
                const Vector3 p = n * radius;

                const Vector3 dPdU = {-sinT * sinP, 0.0f, sinT * cosP};
                const Vector3 dPdV = {cosT * cosP, -sinT, cosT * sinP};
                const Vector4 tangent = MakeTangent(n, dPdU, dPdV);

                EmitVertex(out, p, n, {tangent.x, tangent.y, tangent.z}, tangent.w, {u, v});
            }
        }

        const uint32_t row = sectors + 1;
        for (uint32_t i = 0; i < rings; ++i) {
            for (uint32_t j = 0; j < sectors; ++j) {
                const uint32_t a = i * row + j;
                const uint32_t b = i * row + j + 1;
                const uint32_t c = (i + 1) * row + j + 1;
                const uint32_t d = (i + 1) * row + j;
                EmitTriangle(out, a, c, b);
                EmitTriangle(out, a, d, c);
            }
        }
        return out;
    }

    inline GeometryStreams GenerateCylinder(float radiusTop, float radiusBottom, float height, uint32_t sectors)
    {
        using namespace geometry_detail;
        GeometryStreams out;
        const float halfH = height * 0.5f;

        // side
        for (uint32_t j = 0; j <= sectors; ++j) {
            const float phi  = 2.0f * PI * static_cast<float>(j) / static_cast<float>(sectors);
            const float u    = static_cast<float>(j) / static_cast<float>(sectors);
            const float sinP = std::sin(phi);
            const float cosP = std::cos(phi);
            const Vector3 n  = {cosP, 0.0f, sinP};
            const Vector3 t  = {-sinP, 0.0f, cosP};

            const Vector3 pTop = {radiusTop * cosP, halfH, radiusTop * sinP};
            const Vector3 pBot = {radiusBottom * cosP, -halfH, radiusBottom * sinP};
            EmitVertex(out, pBot, n, t, 1.0f, {u, 0.0f});
            EmitVertex(out, pTop, n, t, 1.0f, {u, 1.0f});
        }

        // caps (triangle fan)
        auto EmitCap = [&](float y, bool top) {
            const float r    = top ? radiusTop : radiusBottom;
            const Vector3 n  = top ? VEC3_Y : -VEC3_Y;
            const float base = static_cast<uint32_t>(out.positions.size());
            EmitVertex(out, {0.0f, y, 0.0f}, n, VEC3_X, 1.0f, {0.5f, 0.5f});
            for (uint32_t j = 0; j <= sectors; ++j) {
                const float phi  = 2.0f * PI * static_cast<float>(j) / static_cast<float>(sectors);
                const float cosP = std::cos(phi);
                const float sinP = std::sin(phi);
                EmitVertex(out, {r * cosP, y, r * sinP}, n, VEC3_X, 1.0f,
                           {0.5f + 0.5f * cosP, 0.5f + (top ? -0.5f : 0.5f) * sinP});
            }
            for (uint32_t j = 0; j < sectors; ++j) {
                if (top) {
                    EmitTriangle(out, base, base + 1 + j + 1, base + 1 + j);
                } else {
                    EmitTriangle(out, base, base + 1 + j, base + 1 + j + 1);
                }
            }
        };
        EmitCap(halfH, true);
        EmitCap(-halfH, false);

        // side triangles (side vertices are 0..2*(sectors+1)-1: bot=2j, top=2j+1)
        for (uint32_t j = 0; j < sectors; ++j) {
            const uint32_t b0 = 2 * j;
            const uint32_t t0 = 2 * j + 1;
            const uint32_t b1 = 2 * j + 2;
            const uint32_t t1 = 2 * j + 3;
            EmitTriangle(out, b0, t1, t0);
            EmitTriangle(out, b0, b1, t1);
        }

        return out;
    }

    inline GeometryStreams GenerateCone(float radius, float height, uint32_t sectors)
    {
        using namespace geometry_detail;
        GeometryStreams out;
        const float halfH = height * 0.5f;

        // apex (top, radius 0) + base ring (bottom)
        const Vector3 apexN = {0.0f, 1.0f, 0.0f};
        const uint32_t apex = EmitVertex(out, {0.0f, halfH, 0.0f}, apexN, VEC3_X, 1.0f, {0.5f, 0.5f});
        for (uint32_t j = 0; j <= sectors; ++j) {
            const float phi  = 2.0f * PI * static_cast<float>(j) / static_cast<float>(sectors);
            const float cosP = std::cos(phi);
            const float sinP = std::sin(phi);
            EmitVertex(out, {radius * cosP, -halfH, radius * sinP}, {0.0f, -1.0f, 0.0f}, VEC3_X, 1.0f,
                       {0.5f + 0.5f * cosP, 0.5f + 0.5f * sinP});
        }

        // side triangles
        for (uint32_t j = 0; j < sectors; ++j) {
            EmitTriangle(out, apex, apex + 1 + j, apex + 1 + j + 1);
        }

        // bottom cap
        const uint32_t capBase = static_cast<uint32_t>(out.positions.size());
        EmitVertex(out, {0.0f, -halfH, 0.0f}, {0.0f, -1.0f, 0.0f}, VEC3_X, 1.0f, {0.5f, 0.5f});
        for (uint32_t j = 0; j <= sectors; ++j) {
            const float phi  = 2.0f * PI * static_cast<float>(j) / static_cast<float>(sectors);
            const float cosP = std::cos(phi);
            const float sinP = std::sin(phi);
            EmitVertex(out, {radius * cosP, -halfH, radius * sinP}, {0.0f, -1.0f, 0.0f}, VEC3_X, 1.0f,
                       {0.5f + 0.5f * cosP, 0.5f + 0.5f * sinP});
        }
        for (uint32_t j = 0; j < sectors; ++j) {
            EmitTriangle(out, capBase, capBase + 1 + j, capBase + 1 + j + 1);
        }

        return out;
    }

    inline GeometryStreams GenerateCapsule(float radius, float height, uint32_t rings, uint32_t sectors)
    {
        using namespace geometry_detail;
        GeometryStreams out;

        // top hemisphere (theta 0..PI/2, from +Y apex to equator)
        for (uint32_t i = 0; i <= rings / 2; ++i) {
            const float theta = PI * 0.5f * static_cast<float>(i) / static_cast<float>(rings / 2);
            const float v     = static_cast<float>(i) / static_cast<float>(rings / 2);
            const float sinT  = std::sin(theta);
            const float cosT  = std::cos(theta);
            for (uint32_t j = 0; j <= sectors; ++j) {
                const float phi  = 2.0f * PI * static_cast<float>(j) / static_cast<float>(sectors);
                const float u    = static_cast<float>(j) / static_cast<float>(sectors);
                const float sinP = std::sin(phi);
                const float cosP = std::cos(phi);

                const Vector3 n = {sinT * cosP, cosT, sinT * sinP};
                const Vector3 p = n * radius + Vector3(0.0f, height * 0.5f, 0.0f);
                const Vector3 dPdU = {-sinT * sinP, 0.0f, sinT * cosP};
                const Vector3 dPdV = {cosT * cosP, -sinT, cosT * sinP};
                const Vector4 tangent = MakeTangent(n, dPdU, dPdV);

                EmitVertex(out, p, n, {tangent.x, tangent.y, tangent.z}, tangent.w, {u, v});
            }
        }

        // middle cylinder (equator to equator)
        for (uint32_t j = 0; j <= sectors; ++j) {
            const float phi  = 2.0f * PI * static_cast<float>(j) / static_cast<float>(sectors);
            const float u    = static_cast<float>(j) / static_cast<float>(sectors);
            const float sinP = std::sin(phi);
            const float cosP = std::cos(phi);
            const Vector3 n  = {cosP, 0.0f, sinP};
            const Vector3 t  = {-sinP, 0.0f, cosP};

            const Vector3 pTop = {radius * cosP, height * 0.5f, radius * sinP};
            const Vector3 pBot = {radius * cosP, -height * 0.5f, radius * sinP};
            EmitVertex(out, pBot, n, t, 1.0f, {u, 0.0f});
            EmitVertex(out, pTop, n, t, 1.0f, {u, 1.0f});
        }

        // bottom hemisphere (theta PI/2..PI)
        for (uint32_t i = 0; i <= rings / 2; ++i) {
            const float theta = PI * 0.5f + PI * 0.5f * static_cast<float>(i) / static_cast<float>(rings / 2);
            const float v     = 0.5f + static_cast<float>(i) / static_cast<float>(rings / 2) * 0.5f;
            const float sinT  = std::sin(theta);
            const float cosT  = std::cos(theta);
            for (uint32_t j = 0; j <= sectors; ++j) {
                const float phi  = 2.0f * PI * static_cast<float>(j) / static_cast<float>(sectors);
                const float u    = static_cast<float>(j) / static_cast<float>(sectors);
                const float sinP = std::sin(phi);
                const float cosP = std::cos(phi);

                const Vector3 n = {sinT * cosP, cosT, sinT * sinP};
                const Vector3 p = n * radius + Vector3(0.0f, -height * 0.5f, 0.0f);
                const Vector3 dPdU = {-sinT * sinP, 0.0f, sinT * cosP};
                const Vector3 dPdV = {cosT * cosP, -sinT, cosT * sinP};
                const Vector4 tangent = MakeTangent(n, dPdU, dPdV);

                EmitVertex(out, p, n, {tangent.x, tangent.y, tangent.z}, tangent.w, {u, v});
            }
        }

        // indices. Layout: top hemisphere (R+1 rows), middle (2 rows),
        // bottom hemisphere (R+1 rows), where R = rings/2, row = sectors+1.
        const uint32_t row  = sectors + 1;
        const uint32_t R    = rings / 2;
        const uint32_t topRows = R + 1;
        const uint32_t midBase = topRows * row;        // 2 rows for middle
        const uint32_t botBase = midBase + 2 * row;    // bottom hemisphere

        auto EmitGrid = [&](uint32_t base, uint32_t rows) {
            for (uint32_t i = 0; i + 1 < rows; ++i) {
                for (uint32_t j = 0; j < sectors; ++j) {
                    const uint32_t a = base + i * row + j;
                    const uint32_t b = base + i * row + j + 1;
                    const uint32_t c = base + (i + 1) * row + j + 1;
                    const uint32_t d = base + (i + 1) * row + j;
                    EmitTriangle(out, a, c, b);
                    EmitTriangle(out, a, d, c);
                }
            }
        };
        EmitGrid(0, topRows);                 // top hemisphere
        EmitGrid(midBase, 2);                 // middle cylinder
        EmitGrid(botBase, R + 1);             // bottom hemisphere

        return out;
    }

} // namespace sky

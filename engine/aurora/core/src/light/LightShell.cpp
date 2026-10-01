//
// Aurora light shell implementation: influence volumes, conservative world /
// screen bounds, convex-hull shell geometry and tile coverage.
//

#include <aurora/light/LightShell.h>
#include <aurora/light/LightSystem.h>
#include <aurora/scene/SceneTypes.h>
#include <aurora/scene/SceneView.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace sky::aurora {

    namespace {

        constexpr float kBigDirectionalExtent = 1.0e5f;
        constexpr float kMinW                 = 1.0e-6f;

        AABB DefaultDirectionalBox(const Vector3 &center)
        {
            const Vector3 extent(kBigDirectionalExtent);
            return AABB(center - extent, center + extent);
        }

        // World AABB of the view frustum via the inverse view-projection.
        AABB ComputeViewFrustumWorldBox(const SceneView &view)
        {
            const Matrix4 inv = view.GetViewProjectMatrix().Inverse();

            static const float kCorner[8] = {-1.f, 1.f, -1.f, 1.f, -1.f, 1.f, -1.f, 1.f};

            AABB box;
            bool first = true;
            for (uint32_t i = 0; i < 8; ++i) {
                const float x = ((i & 1u) != 0u) ? 1.f : -1.f;
                const float y = ((i & 2u) != 0u) ? 1.f : -1.f;
                const float z = ((i & 4u) != 0u) ? 1.f : -1.f;
                (void)kCorner;

                const Vector4 p = inv * Vector4(x, y, z, 1.f);
                if (std::abs(p.w) < kMinW) {
                    continue;
                }
                const float   invW = 1.f / p.w;
                const Vector3 world(p.x * invW, p.y * invW, p.z * invW);

                if (first) {
                    box.min = box.max = world;
                    first             = false;
                } else {
                    box.min = Vector3(std::fmin(box.min.x, world.x), std::fmin(box.min.y, world.y), std::fmin(box.min.z, world.z));
                    box.max = Vector3(std::fmax(box.max.x, world.x), std::fmax(box.max.y, world.y), std::fmax(box.max.z, world.z));
                }
            }

            if (first) {
                return DefaultDirectionalBox(VEC3_ZERO);
            }
            return box;
        }

        void BuildOrthoBasis(const Vector3 &axis, Vector3 &u, Vector3 &v)
        {
            const Vector3 reference = (std::abs(axis.y) < 0.99f) ? VEC3_Y : VEC3_X;
            u                       = reference.Cross(axis);
            u.Normalize();
            v = axis.Cross(u);
            v.Normalize();
        }

        bool ProjectToScreen(const Vector4 &clip, const Vector2 &extent, Vector2 &out)
        {
            if (clip.w <= kMinW) {
                return false;
            }
            const float invW = 1.f / clip.w;
            out.x            = (clip.x * invW * 0.5f + 0.5f) * extent.x;
            out.y            = (0.5f - clip.y * invW * 0.5f) * extent.y;
            return true;
        }

    } // namespace

    AABB ComputeLightBounds(const Light &light, const WorldInfo &world)
    {
        const Vector3 position = ExtractLightPosition(world.world);

        switch (light.type) {
        case LightType::POINT: {
            const Vector3 extent(light.range);
            return AABB(position - extent, position + extent);
        }
        case LightType::SPOT: {
            const Vector3 direction  = ExtractLightDirection(world.world);
            const float   height     = light.range;
            const float   baseRadius = height * std::tan(light.outerConeAngle);
            const Vector3 baseCenter = position + direction * height;

            const float rx = baseRadius * std::sqrt(std::fmax(0.f, 1.f - direction.x * direction.x));
            const float ry = baseRadius * std::sqrt(std::fmax(0.f, 1.f - direction.y * direction.y));
            const float rz = baseRadius * std::sqrt(std::fmax(0.f, 1.f - direction.z * direction.z));

            const Vector3 baseMin = baseCenter - Vector3(rx, ry, rz);
            const Vector3 baseMax = baseCenter + Vector3(rx, ry, rz);

            return AABB(Vector3(std::fmin(position.x, baseMin.x), std::fmin(position.y, baseMin.y), std::fmin(position.z, baseMin.z)),
                        Vector3(std::fmax(position.x, baseMax.x), std::fmax(position.y, baseMax.y), std::fmax(position.z, baseMax.z)));
        }
        case LightType::DIRECTIONAL:
        default: return DefaultDirectionalBox(position);
        }
    }

    LightVolume ComputeLightVolume(const Light &light, const WorldInfo &world, const SceneView *view)
    {
        LightVolume   volume;
        const Vector3 position = ExtractLightPosition(world.world);

        switch (light.type) {
        case LightType::POINT:
            volume.type   = LightVolumeType::SPHERE;
            volume.center = position;
            volume.radius = light.range;
            volume.bounds = ComputeLightBounds(light, world);
            break;

        case LightType::SPOT:
            volume.type       = LightVolumeType::CONE;
            volume.apex       = position;
            volume.axis       = ExtractLightDirection(world.world);
            volume.height     = light.range;
            volume.baseRadius = light.range * std::tan(light.outerConeAngle);
            volume.bounds     = ComputeLightBounds(light, world);
            break;

        case LightType::DIRECTIONAL:
        default:
            volume.type   = LightVolumeType::BOX;
            volume.box    = (view != nullptr) ? ComputeViewFrustumWorldBox(*view) : DefaultDirectionalBox(position);
            volume.bounds = volume.box;
            break;
        }

        return volume;
    }

    GeometryStreams GenerateLightShellGeometry(const LightVolume &volume, uint32_t sectors, uint32_t rings)
    {
        using namespace geometry_detail;

        GeometryStreams out;
        if (sectors == 0) {
            sectors = 1;
        }
        if (rings == 0) {
            rings = 1;
        }

        switch (volume.type) {
        case LightVolumeType::SPHERE: {
            out = GenerateSphere(volume.radius, rings, sectors);
            for (auto &p : out.positions) {
                p += volume.center;
            }
            break;
        }

        case LightVolumeType::CONE: {
            Vector3 axis = volume.axis;
            if (axis.Length() < kMinW) {
                axis = VEC3_NZ;
            } else {
                axis.Normalize();
            }

            Vector3 u;
            Vector3 v;
            BuildOrthoBasis(axis, u, v);

            const Vector3 baseCenter = volume.apex + axis * volume.height;

            const uint32_t apex = EmitVertex(out, volume.apex, -axis, VEC3_X, 1.f, {0.5f, 0.f});
            for (uint32_t j = 0; j <= sectors; ++j) {
                const float   phi    = 2.f * PI * static_cast<float>(j) / static_cast<float>(sectors);
                const Vector3 radial = u * std::cos(phi) + v * std::sin(phi);
                const Vector3 p      = baseCenter + radial * volume.baseRadius;
                EmitVertex(out, p, radial, u, 1.f, {static_cast<float>(j) / static_cast<float>(sectors), 1.f});
            }
            for (uint32_t j = 0; j < sectors; ++j) {
                EmitTriangle(out, apex, apex + 1 + j, apex + 1 + j + 1);
            }

            // base cap
            const uint32_t cap = EmitVertex(out, baseCenter, -axis, VEC3_X, 1.f, {0.5f, 0.5f});
            for (uint32_t j = 0; j <= sectors; ++j) {
                const float   phi    = 2.f * PI * static_cast<float>(j) / static_cast<float>(sectors);
                const Vector3 radial = u * std::cos(phi) + v * std::sin(phi);
                EmitVertex(out, baseCenter + radial * volume.baseRadius, -axis, u, 1.f, {0.5f, 0.5f});
            }
            for (uint32_t j = 0; j < sectors; ++j) {
                EmitTriangle(out, cap, cap + 1 + j + 1, cap + 1 + j);
            }
            break;
        }

        case LightVolumeType::BOX: {
            const Vector3 center     = (volume.box.min + volume.box.max) * 0.5f;
            const Vector3 halfExtent = (volume.box.max - volume.box.min) * 0.5f;

            out = GenerateCube(2.f); // -1..1
            for (auto &p : out.positions) {
                p = center + p * halfExtent;
            }
            break;
        }

        case LightVolumeType::NONE:
        default: break;
        }

        return out;
    }

    ScreenRect ComputeConservativeScreenRect(const LightVolume &volume, const Matrix4 &viewProj, const Vector2 &extent)
    {
        ScreenRect rect;

        if (volume.type == LightVolumeType::NONE) {
            return rect;
        }
        if (volume.type == LightVolumeType::BOX) {
            rect.minX  = 0.f;
            rect.minY  = 0.f;
            rect.maxX  = extent.x;
            rect.maxY  = extent.y;
            rect.valid = false;
            return rect;
        }

        float minX = std::numeric_limits<float>::max();
        float minY = std::numeric_limits<float>::max();
        float maxX = -std::numeric_limits<float>::max();
        float maxY = -std::numeric_limits<float>::max();
        bool  ok   = true;

        for (uint32_t i = 0; i < 8; ++i) {
            const float x = ((i & 1u) != 0u) ? volume.bounds.max.x : volume.bounds.min.x;
            const float y = ((i & 2u) != 0u) ? volume.bounds.max.y : volume.bounds.min.y;
            const float z = ((i & 4u) != 0u) ? volume.bounds.max.z : volume.bounds.min.z;

            const Vector4 clip = viewProj * Vector4(x, y, z, 1.f);
            Vector2       s{};
            if (!ProjectToScreen(clip, extent, s)) {
                ok = false;
                break;
            }
            minX = std::fmin(minX, s.x);
            minY = std::fmin(minY, s.y);
            maxX = std::fmax(maxX, s.x);
            maxY = std::fmax(maxY, s.y);
        }

        if (!ok) {
            rect.minX  = 0.f;
            rect.minY  = 0.f;
            rect.maxX  = extent.x;
            rect.maxY  = extent.y;
            rect.valid = false;
            return rect;
        }

        rect.minX  = std::fmax(0.f, minX);
        rect.minY  = std::fmax(0.f, minY);
        rect.maxX  = std::fmin(extent.x, maxX);
        rect.maxY  = std::fmin(extent.y, maxY);
        rect.valid = true;
        return rect;
    }

    LightCoverage ComputeConservativeCoverage(const LightVolume &volume, const Matrix4 &viewProj, const Vector2 &extent, uint32_t tileSize)
    {
        LightCoverage coverage;
        if (tileSize == 0) {
            tileSize = 16;
        }
        if (extent.x <= 0.f || extent.y <= 0.f) {
            return coverage;
        }

        coverage.tilesX = static_cast<uint32_t>(std::ceil(extent.x / static_cast<float>(tileSize)));
        coverage.tilesY = static_cast<uint32_t>(std::ceil(extent.y / static_cast<float>(tileSize)));

        if (volume.type == LightVolumeType::NONE || coverage.tilesX == 0 || coverage.tilesY == 0) {
            return coverage;
        }

        const uint32_t       total = coverage.tilesX * coverage.tilesY;
        std::vector<uint8_t> hit(total, 0);
        const auto           markAll = [&]() { std::fill(hit.begin(), hit.end(), static_cast<uint8_t>(1)); };

        if (volume.type == LightVolumeType::BOX) {
            markAll();
        } else {
            const GeometryStreams shell = GenerateLightShellGeometry(volume);
            if (shell.positions.empty()) {
                return coverage;
            }

            std::vector<Vector2> screen(shell.positions.size());
            bool                 behind = false;
            for (size_t i = 0; i < shell.positions.size(); ++i) {
                const Vector3 &p    = shell.positions[i];
                const Vector4  clip = viewProj * Vector4(p.x, p.y, p.z, 1.f);
                if (!ProjectToScreen(clip, extent, screen[i])) {
                    behind = true;
                    break;
                }
            }

            if (behind) {
                markAll();
            } else {
                const float maxX = extent.x - 1e-3f;
                const float maxY = extent.y - 1e-3f;

                for (size_t i = 0; i + 2 < shell.indices.size(); i += 3) {
                    const Vector2 &a = screen[shell.indices[i]];
                    const Vector2 &b = screen[shell.indices[i + 1]];
                    const Vector2 &c = screen[shell.indices[i + 2]];

                    const float triMinX = std::fmin(a.x, std::fmin(b.x, c.x));
                    const float triMinY = std::fmin(a.y, std::fmin(b.y, c.y));
                    const float triMaxX = std::fmax(a.x, std::fmax(b.x, c.x));
                    const float triMaxY = std::fmax(a.y, std::fmax(b.y, c.y));

                    if (triMaxX < 0.f || triMaxY < 0.f || triMinX > extent.x || triMinY > extent.y) {
                        continue;
                    }

                    const float cx0 = std::fmax(triMinX, 0.f);
                    const float cy0 = std::fmax(triMinY, 0.f);
                    const float cx1 = std::fmin(triMaxX, maxX);
                    const float cy1 = std::fmin(triMaxY, maxY);

                    const uint32_t tx0 = static_cast<uint32_t>(cx0 / static_cast<float>(tileSize));
                    const uint32_t ty0 = static_cast<uint32_t>(cy0 / static_cast<float>(tileSize));
                    const uint32_t tx1 = std::min(static_cast<uint32_t>(cx1 / static_cast<float>(tileSize)), coverage.tilesX - 1);
                    const uint32_t ty1 = std::min(static_cast<uint32_t>(cy1 / static_cast<float>(tileSize)), coverage.tilesY - 1);

                    for (uint32_t ty = ty0; ty <= ty1; ++ty) {
                        for (uint32_t tx = tx0; tx <= tx1; ++tx) {
                            hit[ty * coverage.tilesX + tx] = 1;
                        }
                    }
                }
            }
        }

        for (uint32_t i = 0; i < total; ++i) {
            if (hit[i] != 0u) {
                coverage.tiles.push_back(i);
            }
        }

        return coverage;
    }

} // namespace sky::aurora

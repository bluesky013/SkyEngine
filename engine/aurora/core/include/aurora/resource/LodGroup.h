//
// LodGroup: ordered level-of-detail table for a mesh instance. Each level
// binds a screen-size threshold to a Mesh. It aggregates Mesh references only
// (no GPU resource of its own), so it derives RefObject like Mesh/Material and
// stays framework-free (no Uuid / AssetManager).
//

#pragma once

#include <aurora/resource/Mesh.h>
#include <core/math/MathUtil.h>
#include <core/shapes/Bounds.h>
#include <core/template/ReferenceObject.h>

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace sky::aurora {

    struct LodLevel {
        float            screenSize = 1.f;
        CounterPtr<Mesh> mesh;
    };

    class LodGroup : public RefObject {
    public:
        LodGroup() = default;
        ~LodGroup() override = default;

        LodGroup(const LodGroup &) = delete;
        LodGroup &operator=(const LodGroup &) = delete;

        void AddLevel(LodLevel level)
        {
            levels.push_back(std::move(level));
        }

        const std::vector<LodLevel> &GetLevels() const { return levels; }
        uint32_t                     GetLevelCount() const { return static_cast<uint32_t>(levels.size()); }

        CounterPtr<Mesh> GetMesh(uint32_t level) const
        {
            if (level >= levels.size()) {
                return {};
            }
            return levels[level].mesh;
        }

        // Level 0 mesh local bounds; an empty group or null mesh yields an empty
        // sphere rather than a null dereference.
        BoundingBoxSphere GetBoundingSphere() const
        {
            if (levels.empty() || levels[0].mesh == nullptr) {
                return {};
            }
            return BoundingBoxSphere(levels[0].mesh->GetLocalBounds());
        }

        // Levels are expected ordered finest-to-coarsest (descending screenSize).
        // Picks the coarsest level whose threshold still covers the given screen
        // size, defaulting to level 0 when nothing matches.
        uint32_t SelectLod(float screenSize) const
        {
            uint32_t selected = 0;
            const auto count  = static_cast<int32_t>(levels.size());
            for (int32_t level = count - 1; level >= 0; --level) {
                if (levels[level].mesh == nullptr) {
                    continue;
                }
                if (levels[level].screenSize >= screenSize) {
                    selected = static_cast<uint32_t>(level);
                    break;
                }
            }
            return selected;
        }

        uint32_t SelectLod(const BoundingBoxSphere &bounds, const Vector3 &viewOrigin, const Matrix4 &proj) const
        {
            return SelectLod(CalculateScreenSize(bounds, viewOrigin, proj));
        }

        // Projected object screen size (fraction of the screen). Mirrors legacy
        // render::LodUtils::CalculateScreenSizeByBound.
        static float CalculateScreenSize(const BoundingBoxSphere &bounds, const Vector3 &viewOrigin, const Matrix4 &proj)
        {
            const float dist           = Length(bounds.center - viewOrigin);
            const float screenMultiple = std::max(0.5f * proj.m[0][0], 0.5f * proj.m[1][1]);
            const float screenRadius   = screenMultiple * bounds.radius / std::max(1.0f, dist);
            return screenRadius * 2.0f;
        }

    private:
        std::vector<LodLevel> levels;
    };

} // namespace sky::aurora

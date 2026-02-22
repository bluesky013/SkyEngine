//
// Created by blues on 2026/2/16.
//

#include <render/lod/LODGroup.h>
#include <render/lod/LodUtils.h>
#include <core/util/Cast.h>
#include <core/math/MathUtil.h>

namespace sky {

    void LodGroup::Init(size_t lodNum) noexcept
    {
        lodProxys.resize(lodNum);
    }

    void LodGroup::AddLod(LodProxy* proxy, uint32_t lodIndex) noexcept
    {
        lodProxys[lodIndex].reset(proxy);
    }

    uint32_t LodGroup::SelectLod(const Vector3& origin, float sphereRadius, const Vector3& viewOrigin, const Matrix4& proj) const noexcept
    {
        const auto numLod = Cast<int32_t>(lodProxys.size());

        const float screenSizeSquared = CalculateScreenSizeSquired(origin, sphereRadius, viewOrigin, proj);
        const float scale = config.scaleFactor;

        uint32_t selectedLod = config.lodBias;

        for (int32_t lod = numLod - 1; lod >= 0; --lod) {
            const auto* proxy = lodProxys[lod].get();

            if (proxy == nullptr) {
                continue;
            }

            const float meshScreenSize = proxy->GetLevel().screenSize * scale;
            const float meshScreenSizeSquared = Square(meshScreenSize * 0.5f);

            if (meshScreenSizeSquared >= screenSizeSquared) {
                selectedLod = std::max(Cast<uint32_t>(lod), config.lodBias);
                break;
            }
        }

        return selectedLod;
    }

    uint32_t LodGroup::GetMaxSectionNumWithSkin() const noexcept
    {
        uint32_t sectionNum = 0;
        for (auto& proxy : lodProxys) {
            if (!proxy->HasSkin()) {
                continue;
            }
            sectionNum = std::max(proxy->GetSectionNum(), sectionNum);
        }

        return sectionNum;
    }

} // namespace sky
//
// Created on 2026/11/15.
//

#include <aurora/adaptor/assets/SkinResource.h>

#include <animation/core/Skeleton.h>

namespace sky::aurora {
    CounterPtr<Skin> FetchOrCreateSkinByAsset(const std::shared_ptr<Asset<Skin>> &asset, SkinStatus *status)
    {
        const auto fail = [status](SkinStatus error) -> CounterPtr<Skin> {
            if (status != nullptr) {
                *status = std::move(error);
            }
            return {};
        };
        if (!asset) {
            return fail({SkinError::InvalidData, "Skin asset is missing"});
        }
        if (asset->Data().version != SkinAssetData::CURRENT_VERSION) {
            return fail({SkinError::UnsupportedVersion, "obsolete AuroraSkin product; recook the owner source"});
        }
        if (!asset->Data().validation) {
            return fail(asset->Data().validation);
        }
        if (status != nullptr) {
            *status = {};
        }
        return asset->GetOrCreateResource([status](Asset<Skin> &value) { return Skin::Create(value.Data().binding, status); });
    }

    SkinStatus MakeSkinTargetLayout(const Skeleton &skeleton, SkinTargetLayout &out)
    {
        SkinTargetLayout target;
        target.signature  = skeleton.GetLayoutSignature();
        const auto status = skeleton.GetRefPos()->ToSkinRenderData(target.referenceModels, Transform::GetIdentity());
        if (!status) {
            return {SkinError::InvalidData, status.message};
        }
        target.paths.reserve(skeleton.GetNumBones());
        for (size_t i = 0; i < skeleton.GetNumBones(); ++i) {
            target.paths.emplace_back(skeleton.GetBoneByIndex(static_cast<BoneIndex>(i))->name.GetStr());
        }
        out = std::move(target);
        return {};
    }

} // namespace sky::aurora

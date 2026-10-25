#include <aurora/adaptor/assets/SkinAsset.h>

#include <animation/core/Skeleton.h>
#include <framework/serialization/BinaryArchive.h>

namespace sky::aurora {

    void SkinAssetData::clear()
    {
        sourceOwner = {};
        binding     = {};
        validation  = {};
    }

    void SkinAssetData::Save(BinaryOutputArchive &ar) const
    {
        ar.SaveValue(version);
        ar.SaveValue(sourceOwner);
        ar.SaveValue(binding.mode);
        ar.SaveValue(binding.input.modelSpaceSignature);
        ar.SaveValue(binding.targetLayoutSignature);
        ar.SaveValue(static_cast<uint32_t>(binding.input.slots.size()));
        for (const auto &slot : binding.input.slots) {
            ar.SaveValue(slot);
        }
        ar.SaveValue(static_cast<uint32_t>(binding.binds.size()));
        for (const auto &bind : binding.binds) {
            ar.SaveValue(bind.targetBone);
            ar.SaveValue(bind.targetIndex);
            ar.SaveValue(reinterpret_cast<const char *>(bind.inverseBind.v), sizeof(bind.inverseBind.v));
        }
    }

    void SkinAssetData::Load(BinaryInputArchive &ar)
    {
        clear();
        ar.LoadValue(version);
        if (version != CURRENT_VERSION) {
            validation = {SkinError::UnsupportedVersion, "obsolete AuroraSkin product; recook the owner source"};
            return;
        }
        const auto readName = [&ar](std::string &value) {
            uint32_t length = 0;
            ar.LoadValue(length);
            if (!ar.IsValid() || length > 4096) {
                return false;
            }
            value.resize(length);
            if (length != 0) {
                ar.LoadValue(value.data(), length);
            }
            return ar.IsValid() && value.find('\0') == std::string::npos;
        };
        ar.LoadValue(sourceOwner);
        ar.LoadValue(binding.mode);
        ar.LoadValue(binding.input.modelSpaceSignature);
        ar.LoadValue(binding.targetLayoutSignature);
        uint32_t count = 0;
        ar.LoadValue(count);
        if (!ar.IsValid() || count == 0 || count > 256) {
            validation = {SkinError::InvalidData, "invalid Skin input slot count"};
            return;
        }
        binding.input.slots.resize(count);
        for (auto &slot : binding.input.slots) {
            if (!readName(slot)) {
                validation = {SkinError::InvalidData, "invalid Skin input slot name"};
                return;
            }
        }
        ar.LoadValue(count);
        if (!ar.IsValid() || count != binding.input.slots.size()) {
            validation = {SkinError::InvalidData, "Skin bind count differs from its input layout"};
            return;
        }
        binding.binds.resize(count);
        for (auto &bind : binding.binds) {
            if (!readName(bind.targetBone)) {
                validation = {SkinError::InvalidData, "invalid Skin target name"};
                return;
            }
            ar.LoadValue(bind.targetIndex);
            ar.LoadValue(reinterpret_cast<char *>(bind.inverseBind.v), sizeof(bind.inverseBind.v));
        }
        validation = ar.IsValid() ? Skin::Validate(binding) : SkinStatus{SkinError::InvalidData, "truncated Skin binding data"};
    }

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

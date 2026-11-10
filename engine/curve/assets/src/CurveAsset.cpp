#include <curve/assets/CurveAsset.h>

#include "CurveEnumSchema.h"

#include <framework/asset/AssetManager.h>

namespace sky::curve {
    namespace {
        constexpr uint32_t MAGIC   = 0x56435243;
        constexpr uint32_t VERSION = 1;
    } // namespace

    void CurveAssetData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(MAGIC);
        archive.SaveValue(VERSION);
        archive.SaveValue(version);
        archive.SaveValue(sourceHash);
        archive.SaveValue(curve.channels);
        archive.SaveValue(static_cast<uint32_t>(curve.interpolation));
        archive.SaveValue(static_cast<uint32_t>(curve.preWrap));
        archive.SaveValue(static_cast<uint32_t>(curve.postWrap));
        archive.SaveValue(static_cast<uint32_t>(curve.keys.size()));
        for (const auto &key : curve.keys) {
            archive.SaveValue(key.time);
            archive.SaveValue(key.value.x);
            archive.SaveValue(key.value.y);
            archive.SaveValue(key.value.z);
            archive.SaveValue(key.value.w);
            archive.SaveValue(key.tangentIn.x);
            archive.SaveValue(key.tangentIn.y);
            archive.SaveValue(key.tangentIn.z);
            archive.SaveValue(key.tangentIn.w);
            archive.SaveValue(key.tangentOut.x);
            archive.SaveValue(key.tangentOut.y);
            archive.SaveValue(key.tangentOut.z);
            archive.SaveValue(key.tangentOut.w);
        }
    }

    void CurveAssetData::Load(BinaryInputArchive &archive)
    {
        valid          = false;
        curve          = {};
        uint32_t magic = 0, fileVersion = 0, count = 0, interpolation = 0, preWrap = 0, postWrap = 0;
        archive.LoadValue(magic);
        archive.LoadValue(fileVersion);
        archive.LoadValue(version);
        archive.LoadValue(sourceHash);
        archive.LoadValue(curve.channels);
        archive.LoadValue(interpolation);
        archive.LoadValue(preWrap);
        archive.LoadValue(postWrap);
        archive.LoadValue(count);
        if (!archive.IsValid() || magic != MAGIC || fileVersion != VERSION || count == 0 || count > MAX_CURVE_KEYS ||
            interpolation >= static_cast<uint32_t>(CurveInterpolation::END) || preWrap >= static_cast<uint32_t>(CurveWrap::END) ||
            postWrap >= static_cast<uint32_t>(CurveWrap::END)) {
            return;
        }
        curve.interpolation = static_cast<CurveInterpolation>(interpolation);
        curve.preWrap       = static_cast<CurveWrap>(preWrap);
        curve.postWrap      = static_cast<CurveWrap>(postWrap);
        curve.keys.resize(count);
        for (auto &key : curve.keys) {
            archive.LoadValue(key.time);
            archive.LoadValue(key.value.x);
            archive.LoadValue(key.value.y);
            archive.LoadValue(key.value.z);
            archive.LoadValue(key.value.w);
            archive.LoadValue(key.tangentIn.x);
            archive.LoadValue(key.tangentIn.y);
            archive.LoadValue(key.tangentIn.z);
            archive.LoadValue(key.tangentIn.w);
            archive.LoadValue(key.tangentOut.x);
            archive.LoadValue(key.tangentOut.y);
            archive.LoadValue(key.tangentOut.z);
            archive.LoadValue(key.tangentOut.w);
        }
        if (!archive.IsValid()) {
            curve = {};
            return;
        }
        std::string error;
        if (!curve.Validate(error)) {
            curve = {};
            return;
        }
        valid = true;
    }

    void RegisterCurveAssets(SerializationContext *context)
    {
        context->Register<CurveKey>("CurveKey")
            .Member<&CurveKey::time>("time")
            .Member<&CurveKey::value>("value")
            .Member<&CurveKey::tangentIn>("tangentIn")
            .Member<&CurveKey::tangentOut>("tangentOut");
        auto interpolation = context->Register<CurveInterpolation>("CurveInterpolation");
        for (const auto &entry : detail::INTERPOLATIONS)
            interpolation.Enum(entry.value, entry.label);
        auto wrap = context->Register<CurveWrap>("CurveWrap");
        for (const auto &entry : detail::WRAPS)
            wrap.Enum(entry.value, entry.label);
        context->Register<Curve>("Curve")
            .Member<&Curve::channels>("channels")
            .Member<&Curve::interpolation>("interpolation")
            .Member<&Curve::preWrap>("preWrap")
            .Member<&Curve::postWrap>("postWrap")
            .Member<&Curve::keys>("keys");
        context->Register<CurveAssetData>("CurveAssetData").BinLoad<&CurveAssetData::Load>().BinSave<&CurveAssetData::Save>();
        AssetManager::Get()->RegisterAssetHandler<CurveAsset>();
    }
} // namespace sky::curve

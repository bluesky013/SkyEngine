//
// Aurora skin asset: bind-pose skinning payload (inverse bind matrices + bone
// names), cooked as its own asset and referenced by AuroraMesh assets. Bone
// hierarchy lives in animation assets; the runtime bridge maps `boneNames`
// onto an animation skeleton and fills Skin::boneMatrices.
//

#pragma once

#include <aurora/resource/Skin.h>
#include <core/logger/Logger.h>
#include <core/math/Matrix4.h>
#include <framework/asset/Asset.h>
#include <framework/serialization/BinaryArchive.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sky::aurora {

    struct SkinAssetData {
        static constexpr uint32_t CURRENT_VERSION = 1;

        uint32_t                version = CURRENT_VERSION;
        std::vector<Matrix4>    inverseBindMatrices;
        std::vector<std::string> boneNames;   // index-aligned with inverseBindMatrices
        std::vector<uint32_t>    boneMapping; // optional vertex-bone-slot -> bone index

        void Save(BinaryOutputArchive &ar) const
        {
            ar.SaveValue(version);
            ar.SaveValue(static_cast<uint32_t>(inverseBindMatrices.size()));
            if (!inverseBindMatrices.empty()) {
                ar.SaveValue(reinterpret_cast<const char *>(inverseBindMatrices.data()),
                             inverseBindMatrices.size() * sizeof(Matrix4));
            }
            ar.SaveValue(static_cast<uint32_t>(boneNames.size()));
            for (const auto &name : boneNames) {
                ar.SaveValue(name);
            }
            ar.SaveValue(static_cast<uint32_t>(boneMapping.size()));
            if (!boneMapping.empty()) {
                ar.SaveValue(reinterpret_cast<const char *>(boneMapping.data()),
                             boneMapping.size() * sizeof(uint32_t));
            }
        }

        void Load(BinaryInputArchive &ar)
        {
            ar.LoadValue(version);
            if (version != CURRENT_VERSION) {
                LOG_E("AuroraSkinAsset", "unsupported skin asset version: %u (expected %u)", version, CURRENT_VERSION);
                clear();
                return;
            }

            uint32_t count = 0;
            ar.LoadValue(count);
            inverseBindMatrices.resize(count);
            if (count != 0) {
                ar.LoadValue(reinterpret_cast<char *>(inverseBindMatrices.data()), count * sizeof(Matrix4));
            }
            ar.LoadValue(count);
            boneNames.resize(count);
            for (auto &name : boneNames) {
                ar.LoadValue(name);
            }
            ar.LoadValue(count);
            boneMapping.resize(count);
            if (count != 0) {
                ar.LoadValue(reinterpret_cast<char *>(boneMapping.data()), count * sizeof(uint32_t));
            }
        }

        void clear()
        {
            inverseBindMatrices.clear();
            boneNames.clear();
            boneMapping.clear();
        }
    };

} // namespace sky::aurora

namespace sky {

    template <>
    struct AssetTraits<sky::aurora::Skin> {
        using DataType                                = sky::aurora::SkinAssetData;
        static constexpr std::string_view ASSET_TYPE  = "AuroraSkin";
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };

} // namespace sky

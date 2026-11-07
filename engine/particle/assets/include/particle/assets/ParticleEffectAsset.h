#pragma once

#include <framework/asset/Asset.h>
#include <framework/serialization/SerializationContext.h>
#include <particle/ParticleProgram.h>

#include <cstdint>
#include <vector>

namespace sky::particle {

    struct ParticleEffectAssetData {
        uint32_t                     version    = 1;
        uint64_t                     sourceHash = 0;
        std::vector<ParticleProgram> programs;

        void Save(BinaryOutputArchive &archive) const;
        void Load(BinaryInputArchive &archive);
        bool IsLoadValid() const
        {
            return valid;
        }

        bool valid = false;
    };

    class ParticleEffectAsset final : public RefObject {};

    void RegisterParticleAuthoringTypes(SerializationContext *context);
    void RegisterParticleAssets(SerializationContext *context);

} // namespace sky::particle

namespace sky {
    template <>
    struct AssetTraits<particle::ParticleEffectAsset> {
        using DataType                                   = particle::ParticleEffectAssetData;
        static constexpr std::string_view ASSET_TYPE     = "ParticleEffect";
        static constexpr SerializeType    SERIALIZE_TYPE = SerializeType::BIN;
    };
} // namespace sky

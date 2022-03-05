//
// Created by Zach Lee on 2022/3/5.
//

#pragma once

#include <engine/asset/ShaderAsset.h>
#include <framework/asset/Asset.h>
#include <framework/asset/Resource.h>

namespace sky {

    struct ShaderReference {
        ShaderAssetPtr asset;
        std::string path;
    };

    struct ProgramData {
        std::vector<ShaderReference> shaders;
    };

    class ProgramAsset : public AssetBase {
    public:
        static constexpr Uuid TYPE = Uuid::CreateFromString("e8cb35f2-e3f1-4592-bd63-36969887be0b");

        ProgramAsset(const Uuid& id) : AssetBase(id) {}
        ~ProgramAsset() = default;

        template<class Archive>
        void load(Archive& ar)
        {
            ar(data);
        }

        template<class Archive>
        void save(Archive& ar) const
        {
            ar(data);
        }

        const Uuid& GetType() const override { return TYPE; }

        ProgramData data;
    };

    using ProgramAssetPtr = CounterPtr<ProgramAsset>;
}
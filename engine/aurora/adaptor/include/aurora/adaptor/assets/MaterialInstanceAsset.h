//
// Aurora material instance asset: versioned CPU payload of a MaterialInstance
// (parent material Uuid + value / texture overrides). Runtime-only load.
//

#pragma once

#include <aurora/adaptor/assets/MaterialAsset.h>
#include <aurora/resource/Material.h>
#include <core/util/Uuid.h>
#include <framework/asset/Asset.h>
#include <framework/serialization/BinaryArchive.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sky::aurora {

    class Device;

    struct MaterialInstanceValueOverride {
        std::string           name;
        MaterialPropertyType  type = MaterialPropertyType::FLOAT;
        MaterialPropertyValue value;

        void Save(BinaryOutputArchive &ar) const
        {
            ar.SaveValue(name);
            ar.SaveValue(static_cast<uint32_t>(type));
            detail::SavePropertyValue(ar, value);
        }

        void Load(BinaryInputArchive &ar)
        {
            uint32_t typeValue = 0;
            ar.LoadValue(name);
            ar.LoadValue(typeValue);
            type = static_cast<MaterialPropertyType>(typeValue);
            detail::LoadPropertyValue(ar, value);
        }
    };

    struct MaterialInstanceTextureOverride {
        std::string name;
        Uuid        texture;

        void Save(BinaryOutputArchive &ar) const
        {
            ar.SaveValue(name);
            ar.SaveValue(texture);
        }

        void Load(BinaryInputArchive &ar)
        {
            ar.LoadValue(name);
            ar.LoadValue(texture);
        }
    };

    // Authored `.mati` source: parent material + typed property values (same property schema as a
    // material). The builder bakes it into the value / texture override payloads.
    struct MaterialInstanceSource {
        Uuid                              material;
        std::vector<MaterialPropertyData> properties;
    };

    struct MaterialInstanceAssetData {
        static constexpr uint32_t CURRENT_VERSION = 2;

        uint32_t                                     version = CURRENT_VERSION;
        Uuid                                         material;
        std::vector<MaterialInstanceValueOverride>   valueOverrides;
        std::vector<MaterialInstanceTextureOverride> textureOverrides;

        void Save(BinaryOutputArchive &ar) const
        {
            ar.SaveValue(version);
            ar.SaveValue(material);
            ar.SaveValue(static_cast<uint32_t>(valueOverrides.size()));
            for (const auto &value : valueOverrides) {
                value.Save(ar);
            }
            ar.SaveValue(static_cast<uint32_t>(textureOverrides.size()));
            for (const auto &texture : textureOverrides) {
                texture.Save(ar);
            }
        }

        void Load(BinaryInputArchive &ar)
        {
            ar.LoadValue(version);
            if (version != CURRENT_VERSION) {
                clear();
                return;
            }
            ar.LoadValue(material);
            uint32_t valueCount = 0;
            ar.LoadValue(valueCount);
            valueOverrides.resize(valueCount);
            for (auto &value : valueOverrides) {
                value.Load(ar);
            }
            uint32_t textureCount = 0;
            ar.LoadValue(textureCount);
            textureOverrides.resize(textureCount);
            for (auto &texture : textureOverrides) {
                texture.Load(ar);
            }
        }

        void clear()
        {
            material = Uuid{};
            valueOverrides.clear();
            textureOverrides.clear();
        }
    };

    // Runtime-only build: resolves the parent material asset through AssetManager
    // and applies the value / texture overrides onto a MaterialInstance.
    CounterPtr<MaterialInstance> CreateMaterialInstanceFromAsset(Device *device, const Asset<MaterialInstance> &asset);

} // namespace sky::aurora

namespace sky {

    template <>
    struct AssetTraits<sky::aurora::MaterialInstance> {
        using DataType                                   = sky::aurora::MaterialInstanceAssetData;
        static constexpr std::string_view ASSET_TYPE     = "AuroraMaterialInstance";
        static constexpr SerializeType    SERIALIZE_TYPE = SerializeType::BIN;
    };

} // namespace sky

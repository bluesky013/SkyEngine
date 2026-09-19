//
// Aurora material asset: CPU payload (shader ref + property values/textures)
// for aurora::Material. Runtime-only load (no offline build).
//

#pragma once

#include <aurora/resource/Material.h>
#include <framework/asset/Asset.h>
#include <framework/serialization/BinaryArchive.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sky::aurora {

    struct MaterialPropertyData {
        std::string name;
        uint32_t    type  = 0; // 0 = value, 1 = texture
        float       value[4] = {0.f, 0.f, 0.f, 0.f};
        Uuid        texture;
    };

    struct MaterialAssetData {
        Uuid                            shader;
        std::vector<MaterialPropertyData> properties;

        void Save(BinaryOutputArchive &ar) const
        {
            ar.SaveValue(shader.ToString());
            ar.SaveValue(static_cast<uint32_t>(properties.size()));
            for (const auto &prop : properties) {
                ar.SaveValue(prop.name);
                ar.SaveValue(prop.type);
                for (float v : prop.value) {
                    ar.SaveValue(v);
                }
                ar.SaveValue(prop.texture.ToString());
            }
        }

        void Load(BinaryInputArchive &ar)
        {
            std::string shaderStr;
            ar.LoadValue(shaderStr);
            shader = Uuid::CreateFromString(shaderStr);
            uint32_t count = 0;
            ar.LoadValue(count);
            properties.resize(count);
            for (auto &prop : properties) {
                ar.LoadValue(prop.name);
                ar.LoadValue(prop.type);
                for (float &v : prop.value) {
                    ar.LoadValue(v);
                }
                std::string texStr;
                ar.LoadValue(texStr);
                prop.texture = Uuid::CreateFromString(texStr);
            }
        }
    };

} // namespace sky::aurora

namespace sky {

    template <>
    struct AssetTraits<sky::aurora::Material> {
        using DataType                                = sky::aurora::MaterialAssetData;
        static constexpr std::string_view ASSET_TYPE  = "AuroraMaterial";
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };

} // namespace sky

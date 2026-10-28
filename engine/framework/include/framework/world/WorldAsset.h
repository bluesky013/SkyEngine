#pragma once

#include <framework/asset/Asset.h>
#include <framework/world/World.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sky {
    struct WorldComponentAsset {
        Uuid                 componentType;
        Uuid                 dataType;
        std::vector<uint8_t> payload;
    };

    struct WorldActorAsset {
        Uuid                             uuid;
        std::string                      name;
        std::vector<WorldComponentAsset> components;
    };

    struct WorldSubSystemAsset {
        std::string          name;
        bool                 enabled = true;
        Uuid                 configType;
        std::vector<uint8_t> payload;
    };

    // Payloads contain bounded typed values, independent of live component/config ownership.
    struct WorldAssetData {
        static constexpr uint32_t CURRENT_VERSION = 1;
        static constexpr uint32_t MAX_BYTES       = 64 * 1024 * 1024;
        static constexpr uint32_t MAX_ACTORS      = 65536;
        static constexpr uint32_t MAX_COMPONENTS  = 1024;
        static constexpr uint32_t MAX_SUBSYSTEMS  = 256;
        static constexpr uint32_t MAX_ROOTS       = 65536;
        static constexpr uint32_t MAX_ELEMENTS    = 1048576;
        static constexpr uint32_t MAX_STRING      = 1048576;
        static constexpr uint32_t MAX_DEPTH       = 32;

        std::vector<WorldActorAsset>         actors;
        std::vector<WorldSubSystemAsset>     subSystems;
        std::vector<ComponentAssetReference> roots;

        bool Validate(std::string &error) const;
        void Save(BinaryOutputArchive &archive) const;
        void Load(BinaryInputArchive &archive);
        bool IsLoadValid() const
        {
            return loadError.empty();
        }
        const std::string &GetLoadError() const
        {
            return loadError;
        }

    private:
        std::string loadError;
    };

    bool CookWorldJson(IStreamArchive &source, WorldAssetData &out, std::string &error);
    bool CaptureWorldAsset(const World &world, WorldAssetData &out, std::string &error);
    // Publishes only a validated candidate; subsystems attach before actor component hooks.
    bool InstantiateWorldAsset(const WorldAssetData &data, WorldPtr &out, std::string &error);

    template <>
    struct AssetTraits<World> {
        using DataType                                   = WorldAssetData;
        static constexpr std::string_view ASSET_TYPE     = "World";
        static constexpr SerializeType    SERIALIZE_TYPE = SerializeType::BIN;
    };
} // namespace sky

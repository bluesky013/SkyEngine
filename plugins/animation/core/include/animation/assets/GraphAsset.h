#pragma once

#include <animation/graph/GraphDefinition.h>
#include <framework/asset/Asset.h>

namespace sky { class SerializationContext; }

namespace sky::animation {
    class GraphAsset final : public RefObject {};
    struct GraphAssetData {
        CookedGraph     graph;
        GraphValidation validation;
        bool            IsLoadValid() const
        {
            return static_cast<bool>(validation);
        }
        void Load(BinaryInputArchive &archive);
        void Save(BinaryOutputArchive &archive) const;
        void clear()
        {
            graph      = {};
            validation = {};
        }
    };
    GraphValidation ParseGraphSource(std::string_view text, GraphSource &out, const Uuid &legacyDocument = {});
    GraphValidation WriteGraphSource(const GraphSource &source, std::string &out, bool includeEditor = true);
    void            RegisterGraphAssets(SerializationContext *context);
} // namespace sky::animation
namespace sky {
    template <>
    struct AssetTraits<animation::GraphAsset> {
        using DataType                                   = animation::GraphAssetData;
        static constexpr std::string_view ASSET_TYPE     = "AnimationGraph";
        static constexpr SerializeType    SERIALIZE_TYPE = SerializeType::BIN;
    };
} // namespace sky

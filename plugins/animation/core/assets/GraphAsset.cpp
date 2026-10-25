#include <animation/assets/GraphAsset.h>

#include <framework/asset/AssetManager.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::animation {
    namespace {
        constexpr uint32_t MAGIC     = 0x32524741;
        constexpr uint32_t MAX_BYTES = 8u * 1024u * 1024u;
        GraphValidation    Invalid(const char *text)
        {
            return {{{{}, "", text}}};
        }
        class GraphHandler final : public AssetHandler<GraphAsset> {
        public:
            bool SaveChecked(OStreamArchive &archive, const AssetPtr &base) override
            {
                if (!base || base->GetType() != Name("AnimationGraph")) {
                    return false;
                }
                const auto  asset = std::static_pointer_cast<Asset<GraphAsset>>(base);
                CookedGraph checked;
                if (!CompileGraphDefinition(asset->Data().graph.definition, checked) ||
                    checked.topologicalNodes != asset->Data().graph.topologicalNodes || checked.dependencies != asset->Data().graph.dependencies) {
                    return false;
                }
                return AssetHandler<GraphAsset>::SaveChecked(archive, base);
            }
        };
    } // namespace
    void GraphAssetData::Load(BinaryInputArchive &archive)
    {
        graph          = {};
        validation     = Invalid("Truncated or corrupt Graph product");
        uint32_t magic = 0, version = 0, size = 0;
        archive.LoadValue(magic);
        archive.LoadValue(version);
        archive.LoadValue(size);
        if (magic != MAGIC || version != GRAPH_VERSION) {
            validation = Invalid("Legacy or unsupported Graph product; recook the source");
            return;
        }
        if (!archive.IsValid() || !size || size > MAX_BYTES) {
            return;
        }
        std::string text(size, '\0');
        archive.LoadValue(text.data(), size);
        if (!archive.IsValid()) {
            return;
        }
        GraphSource source;
        validation = ParseGraphSource(text, source);
        if (!validation) {
            return;
        }
        CookedGraph compiled;
        validation = CompileGraphDefinition(source.definition, compiled);
        if (!validation) {
            return;
        }
        uint32_t count = 0;
        archive.LoadValue(count);
        if (!archive.IsValid() || count != compiled.topologicalNodes.size()) {
            validation = Invalid("Graph topology count is invalid");
            return;
        }
        for (const auto expected : compiled.topologicalNodes) {
            uint32_t index = 0;
            archive.LoadValue(index);
            if (!archive.IsValid() || index != expected) {
                validation = Invalid("Graph topology is inconsistent");
                return;
            }
        }
        archive.LoadValue(count);
        if (!archive.IsValid() || count != compiled.dependencies.size()) {
            validation = Invalid("Graph dependency count is invalid");
            return;
        }
        for (const auto &expected : compiled.dependencies) {
            Uuid id;
            archive.LoadValue(id);
            if (!archive.IsValid() || id != expected) {
                validation = Invalid("Graph dependencies are inconsistent");
                return;
            }
        }
        graph      = std::move(compiled);
        validation = {};
    }
    void GraphAssetData::Save(BinaryOutputArchive &archive) const
    {
        GraphSource source;
        source.definition = graph.definition;
        std::string text;
        const auto  valid = WriteGraphSource(source, text, false);
        archive.SaveValue(valid ? MAGIC : 0u);
        archive.SaveValue(GRAPH_VERSION);
        archive.SaveValue(static_cast<uint32_t>(text.size()));
        archive.SaveValue(text.data(), text.size());
        archive.SaveValue(static_cast<uint32_t>(graph.topologicalNodes.size()));
        for (const auto node : graph.topologicalNodes) {
            archive.SaveValue(node);
        }
        archive.SaveValue(static_cast<uint32_t>(graph.dependencies.size()));
        for (const auto &id : graph.dependencies) {
            archive.SaveValue(id);
        }
    }
    void RegisterGraphAssets(SerializationContext *context)
    {
        context->Register<GraphAssetData>("GraphAssetData").BinLoad<&GraphAssetData::Load>().BinSave<&GraphAssetData::Save>();
        AssetManager::Get()->RegisterAssetHandler(AssetTraits<GraphAsset>::ASSET_TYPE, new GraphHandler());
    }
} // namespace sky::animation

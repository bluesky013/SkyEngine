#include "animation/editor/CharacterPreviewView.h"
#include "animation/editor/GraphEditor.h"
#include "animation/editor/GraphProperties.h"
#include <framework/interface/IModule.h>

namespace sky::animation::editor {
    class GraphViewProvider final : public sky::editor::IAssetViewProvider {
    public:
        bool UsesDocumentWindow() const override
        {
            return true;
        }
        sky::editor::AssetViewDocument OpenDocument(const Uuid &asset, const std::string &type) override
        {
            if (type != "AnimationGraph" && type != "AnimationClip" && type != "AuroraSkeleton")
                return {};
            auto preview = std::make_shared<CharacterPreview>(asset, type);
            if (type == "AnimationGraph") {
                auto document = std::make_unique<GraphDocument>(asset);
                document->Load();
                return sky::editor::AssetViewDocument{CreateGraphEditor(std::move(document), std::move(preview))};
            }
            return sky::editor::AssetViewDocument{CreateCharacterEditor(std::move(preview))};
        }
    };
    class AnimationEditorModule final : public IModule {
    public:
        void RegisterMeta() override
        {
            RegisterGraphProperties();
            RegisterCharacterPreviewViewTypes();
        }
        bool Init(const StartArguments &) override
        {
            auto *registry = sky::editor::AssetViewProviderRegistry::Get();
            for (const auto *type : {"AnimationGraph", "AnimationClip", "AuroraSkeleton"})
                if (registry->Find(type))
                    return false;
            for (const auto *type : {"AnimationGraph", "AnimationClip", "AuroraSkeleton"})
                registry->Register(type, &provider);
            return true;
        }
        void Shutdown() override
        {
            auto *registry = sky::editor::AssetViewProviderRegistry::Get();
            for (const auto *type : {"AnimationGraph", "AnimationClip", "AuroraSkeleton"})
                if (registry->Find(type) == &provider)
                    registry->Unregister(type);
        }

    private:
        GraphViewProvider provider;
    };
} // namespace sky::animation::editor
REGISTER_MODULE_TAGGED(sky::animation::editor::AnimationEditorModule)

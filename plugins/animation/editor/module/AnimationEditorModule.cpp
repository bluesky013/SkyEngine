#include "animation/editor/CharacterPreviewView.h"
#include "animation/editor/GraphEditor.h"
#include "animation/editor/GraphProperties.h"
#include <framework/application/ModuleManager.h>
#include <framework/interface/IModule.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>

namespace sky::animation::editor {
    class GraphViewProvider final : public sky::editor::IAssetViewProvider {
    public:
        bool UsesDocumentWindow() const override { return true; }
        sky::editor::AssetViewDocument OpenDocument(const Uuid &asset, const std::string &type) override
        {
            if (type != "AnimationGraph" && type != "AnimationClip" && type != "AuroraSkeleton")
                return {};
            auto *system  = Interface<ISystemNotify>::Get()->GetApi();
            auto *modules = system ? system->GetModuleManager() : nullptr;
            auto  lease   = modules ? modules->RetainModule("Animation.Editor") : nullptr;
            if (!lease)
                return {};
            auto preview = std::make_shared<CharacterPreview>(asset, type, RetainCharacterPreviewModules());
            if (type == "AnimationGraph") {
                auto document = std::make_unique<GraphDocument>(asset);
                document->Load();
                return {std::move(lease), CreateGraphEditor(std::move(document), std::move(preview))};
            }
            return {std::move(lease), CreateCharacterEditor(std::move(preview))};
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

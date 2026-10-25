//
// Aurora editor extension module implementation (registration-only).
//

#include <aurora/editor/AuroraEditorModule.h>

#include <aurora/editor/AuroraAssetViews.h>
#include <aurora/editor/AuroraEditorExtension.h>
#include <editor/core/extension/EditorExtensionRegistry.h>
#include <framework/application/ModuleManager.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/serialization/SerializationContext.h>

#include <memory>

namespace sky::aurora::editor {

    void AuroraEditorModule::RegisterMeta()
    {
        ReflectAuroraAssetViews(SerializationContext::Get());
    }

    bool AuroraEditorModule::Init(const StartArguments &args)
    {
        (void)args;

        // Stage the aurora editor extension; the editor host drains it (SandboxModule::BuildEditor)
        // and calls Register() for the per-asset views + GetIconDirectories() for the icon root.
        auto *system = Interface<ISystemNotify>::Get()->GetApi();
        auto *manager = system ? system->GetModuleManager() : nullptr;
        auto lease = manager ? manager->RetainModule("Aurora.Render.Editor") : nullptr;
        if (!lease) return false;
        sky::editor::EditorExtensionRegistry::Get()->Add(std::make_unique<AuroraEditorExtension>(), std::move(lease));
        return true;
    }

    void AuroraEditorModule::Shutdown()
    {
        UnregisterAuroraAssetViews();
    }

} // namespace sky::aurora::editor

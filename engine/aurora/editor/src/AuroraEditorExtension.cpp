//
// Aurora editor extension (see AuroraEditorExtension.h).
//

#include <aurora/editor/AuroraEditorExtension.h>

#include <aurora/editor/AuroraAssetViews.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/application/ModuleManager.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>

#include <vector>

namespace sky::aurora::editor {

    const char *AuroraEditorExtension::GetName() const
    {
        return "AuroraEditor";
    }

    void AuroraEditorExtension::Register()
    {
        auto *system = Interface<ISystemNotify>::Get()->GetApi();
        auto *manager = system ? system->GetModuleManager() : nullptr;
        auto lease = manager ? manager->RetainModule("Aurora.Render.Editor") : nullptr;
        if (lease)
            RegisterAuroraAssetViews(std::move(lease));
    }

    void AuroraEditorExtension::Unregister()
    {
        UnregisterAuroraAssetViews();
    }

    std::vector<std::string> AuroraEditorExtension::GetIconDirectories() const
    {
        return {"icons/aurora"};
    }

} // namespace sky::aurora::editor

//
// Content editor extension (see ContentEditorExtension.h).
//

#include <content/editor/ContentEditorExtension.h>

#include <content/editor/ContentAssetViews.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/application/ModuleManager.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>

#include <vector>

namespace sky::content::editor {

    const char *ContentEditorExtension::GetName() const
    {
        return "ContentEditor";
    }

    void ContentEditorExtension::Register()
    {
        auto *system = Interface<ISystemNotify>::Get()->GetApi();
        auto *manager = system ? system->GetModuleManager() : nullptr;
        auto lease = manager ? manager->RetainModule("Content.Views") : nullptr;
        if (lease)
            RegisterContentAssetViews(std::move(lease));
    }

    void ContentEditorExtension::Unregister()
    {
        UnregisterContentAssetViews();
    }

    std::vector<std::string> ContentEditorExtension::GetIconDirectories() const
    {
        return {"icons/content"};
    }

} // namespace sky::content::editor

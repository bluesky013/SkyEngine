//
// Content editor extension module implementation (registration-only).
//

#include <content/editor/ContentEditorModule.h>

#include <content/editor/ContentAssetViews.h>
#include <content/editor/ContentEditorExtension.h>
#include <editor/core/extension/EditorExtensionRegistry.h>
#include <framework/serialization/SerializationContext.h>

#include <memory>

namespace sky::content::editor {

    void ContentEditorModule::RegisterMeta()
    {
        ReflectContentAssetViews(SerializationContext::Get());
    }

    bool ContentEditorModule::Init(const StartArguments &args)
    {
        (void)args;

        // Stage the content editor extension; the editor host drains it (SandboxModule::BuildEditor)
        // and calls Register() for the per-asset views + GetIconDirectories() for the icon root.
        sky::editor::EditorExtensionRegistry::Get()->Add(std::make_unique<ContentEditorExtension>());
        return true;
    }

    void ContentEditorModule::Shutdown()
    {
        UnregisterContentAssetViews();
    }

} // namespace sky::content::editor

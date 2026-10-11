//
// Content editor extension (see ContentEditorExtension.h).
//

#include <content/editor/ContentEditorExtension.h>

#include <content/editor/ContentAssetViews.h>
#include <framework/serialization/SerializationContext.h>

#include <vector>

namespace sky::content::editor {

    const char *ContentEditorExtension::GetName() const
    {
        return "ContentEditor";
    }

    void ContentEditorExtension::Register()
    {
        // Idempotent (sky::Once): the content cook/editor modules may each reach this.
        RegisterContentAssetViews(sky::SerializationContext::Get());
    }

    void ContentEditorExtension::Unregister()
    {
        // Reflection + asset-view registration is process-lifetime; nothing to undo.
    }

    std::vector<std::string> ContentEditorExtension::GetIconDirectories() const
    {
        return {"icons/content"};
    }

} // namespace sky::content::editor

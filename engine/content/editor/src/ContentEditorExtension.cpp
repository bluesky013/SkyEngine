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
        RegisterContentAssetViews();
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

//
// Aurora editor extension (see AuroraEditorExtension.h).
//

#include <aurora/editor/AuroraEditorExtension.h>

#include <aurora/editor/AuroraAssetViews.h>
#include <framework/serialization/SerializationContext.h>

#include <vector>

namespace sky::aurora::editor {

    const char *AuroraEditorExtension::GetName() const
    {
        return "AuroraEditor";
    }

    void AuroraEditorExtension::Register()
    {
        // Idempotent (sky::Once): the aurora cook/editor modules may each reach this.
        RegisterAuroraAssetViews(sky::SerializationContext::Get());
    }

    void AuroraEditorExtension::Unregister()
    {
        // Reflection + asset-view registration is process-lifetime; nothing to undo.
    }

    std::vector<std::string> AuroraEditorExtension::GetIconDirectories() const
    {
        return {"icons/aurora"};
    }

} // namespace sky::aurora::editor

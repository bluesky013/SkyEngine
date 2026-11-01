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
        RegisterAuroraAssetViews();
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

//
// Content editor extension module implementation (registration-only).
//

#include <content/editor/ContentEditorModule.h>

#include <content/adaptor/ContentReflection.h>
#include <content/editor/ContentAssetViews.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::content::editor {

    bool ContentEditorModule::Init(const StartArguments &args)
    {
        (void)args;

        auto *context = SerializationContext::Get();
        if (context != nullptr) {
            ContentReflection(context);
        }
        RegisterContentAssetViews(context);
        return true;
    }

    void ContentEditorModule::Shutdown()
    {
    }

} // namespace sky::content::editor

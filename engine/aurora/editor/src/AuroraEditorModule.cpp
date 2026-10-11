//
// Aurora editor extension module implementation (registration-only).
//

#include <aurora/editor/AuroraEditorModule.h>

#include <aurora/adaptor/AuroraReflection.h>
#include <aurora/editor/AuroraAssetViews.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::aurora::editor {

    bool AuroraEditorModule::Init(const StartArguments &args)
    {
        (void)args;

        auto *context = SerializationContext::Get();
        // Register aurora asset handlers / components / reflection for the editor.
        if (context != nullptr) {
            AuroraReflection(context);
        }
        // Register the aurora per-asset view data with the editor.
        RegisterAuroraAssetViews(context);
        return true;
    }

    void AuroraEditorModule::Shutdown()
    {
    }

} // namespace sky::aurora::editor

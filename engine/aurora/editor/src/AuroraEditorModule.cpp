//
// Aurora editor extension module implementation.
//

#include <aurora/editor/AuroraEditorModule.h>

namespace sky::aurora::editor {

    bool AuroraEditorModule::Init(const StartArguments &args)
    {
        if (!AuroraModule::Init(args)) {
            return false;
        }

        // Editor-side extensions (actor creators / asset creators / previews)
        // are registered here, mirroring legacy RenderEditorModule. The base
        // Init already ran AuroraReflection, so the editor sees aurora
        // components/assets through the component extension path.
        return true;
    }

    void AuroraEditorModule::Shutdown()
    {
        AuroraModule::Shutdown();
    }

} // namespace sky::aurora::editor

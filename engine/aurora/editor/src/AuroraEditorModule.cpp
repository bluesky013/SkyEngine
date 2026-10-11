//
// Aurora editor extension module implementation (registration-only).
//

#include <aurora/editor/AuroraEditorModule.h>

#include <aurora/adaptor/AuroraReflection.h>
#include <aurora/editor/AuroraEditorExtension.h>
#include <editor/core/extension/EditorExtensionRegistry.h>
#include <framework/serialization/SerializationContext.h>

#include <memory>

namespace sky::aurora::editor {

    bool AuroraEditorModule::Init(const StartArguments &args)
    {
        (void)args;

        // Reflection must be process-wide before the editor host exists (hub mode, world
        // templates); AuroraReflection is idempotent via sky::Once.
        AuroraReflection(sky::SerializationContext::Get());

        // Stage the aurora editor extension; the editor host drains it (SandboxModule::BuildEditor)
        // and calls Register() for the per-asset views + GetIconDirectories() for the icon root.
        sky::editor::EditorExtensionRegistry::Get()->Add(std::make_unique<AuroraEditorExtension>());
        return true;
    }

    void AuroraEditorModule::Shutdown()
    {
    }

} // namespace sky::aurora::editor

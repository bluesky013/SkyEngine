//
// Aurora editor extension module (registration-only). Loaded by the editor through the
// module config. It does NOT create an aurora device: the editor owns rendering via its
// own host. Init runs AuroraReflection (asset handlers / components / reflection) and
// stages an AuroraEditorExtension for the editor host to drain (per-asset views + icon root).
//

#pragma once

#include <framework/interface/IModule.h>

namespace sky::aurora::editor {

    class AuroraEditorModule : public sky::IModule {
    public:
        AuroraEditorModule()           = default;
        ~AuroraEditorModule() override = default;

        bool Init(const sky::StartArguments &args) override;
        void Shutdown() override;
    };

} // namespace sky::aurora::editor

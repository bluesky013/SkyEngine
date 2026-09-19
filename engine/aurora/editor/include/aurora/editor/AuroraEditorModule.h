//
// Aurora editor extension module. Loaded by the editor through the module
// config (mirrors legacy render/editor RenderEditorModule : RenderModule).
// Its Init runs the base AuroraModule::Init (reflection + device) and registers
// editor-side extensions.
//

#pragma once

#include <aurora/adaptor/AuroraModule.h>

namespace sky::aurora::editor {

    class AuroraEditorModule : public sky::aurora::AuroraModule {
    public:
        AuroraEditorModule()           = default;
        ~AuroraEditorModule() override = default;

        bool Init(const sky::StartArguments &args) override;
        void Shutdown() override;
    };

} // namespace sky::aurora::editor

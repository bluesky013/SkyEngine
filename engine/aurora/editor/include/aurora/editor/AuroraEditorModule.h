//
// Stages the editor extension; persistent and runtime metadata have separate module owners.
//

#pragma once

#include <framework/interface/IModule.h>

namespace sky::aurora::editor {

    class AuroraEditorModule : public sky::IModule {
    public:
        AuroraEditorModule()           = default;
        ~AuroraEditorModule() override = default;

        void RegisterMeta() override;
        bool Init(const sky::StartArguments &args) override;
        void Shutdown() override;
    };

} // namespace sky::aurora::editor

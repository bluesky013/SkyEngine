//
// Content editor extension module (registration-only): registers the content adaptor's
// animation asset types and their per-asset view data with the editor. No device.
//

#pragma once

#include <framework/interface/IModule.h>

namespace sky::content::editor {

    class ContentEditorModule : public sky::IModule {
    public:
        ContentEditorModule()           = default;
        ~ContentEditorModule() override = default;

        bool Init(const sky::StartArguments &args) override;
        void Shutdown() override;
    };

} // namespace sky::content::editor

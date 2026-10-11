//
// Content editor extension module (registration-only): registers the content adaptor's
// animation asset types and stages a ContentEditorExtension for the editor host to drain
// (per-asset views + icon root). No device.
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

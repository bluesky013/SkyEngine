//
// The editor host registers Aurora's asset views and mesh creation defaults for its session.
//

#pragma once

#include <aurora/editor/MeshCreationSettings.h>
#include <editor/core/extension/EditorExtension.h>

#include <string>
#include <vector>

namespace sky::aurora::editor {

    class AuroraEditorExtension : public sky::editor::EditorExtension {
    public:
        const char                   *GetName() const override;
        void                          Register() override;
        void                          Unregister() override;
        sky::editor::EditorEditResult MakePropertyEdit(sky::Actor &, const sky::editor::PropertyDescriptor &, const sky::Any &) override;
        sky::editor::EditorEditResult PrepareComponentAddition(sky::Actor &, sky::ComponentBase &) override;

        std::vector<std::string> GetIconDirectories() const override;

    private:
        MeshCreationSettings     creationSettings;
        std::vector<std::string> creators;
        std::shared_ptr<void>    creationLifetime;
        uint64_t                 settingsRegistration = 0;
    };

} // namespace sky::aurora::editor

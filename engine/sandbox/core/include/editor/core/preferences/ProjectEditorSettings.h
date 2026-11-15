#pragma once

#include <editor/core/preferences/PreferencePageRegistry.h>

namespace sky::editor {
    class ProjectEditorSettings : public Singleton<ProjectEditorSettings> {
    public:
        PreferencePageRegistry &Pages()
        {
            return pages;
        }

        bool IsReady() const
        {
            return ready;
        }
        void SetReady(bool value)
        {
            ready = value;
        }

    private:
        bool                   ready = true;
        PreferencePageRegistry pages;
    };
} // namespace sky::editor

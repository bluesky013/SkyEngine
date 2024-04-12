//
// Created by blues on 2024/3/17.
//

#pragma once

#include <core/environment/Singleton.h>
#include <editor/WidgetManager.h>
#include <editor/widgets/Menu.h>
#include <memory>

namespace sky::editor {

    class EditorInstance : public Singleton<EditorInstance> {
    public:
        EditorInstance() = default;
        ~EditorInstance() override = default;

        void Init(ImGuiInstance *instance);

    private:
        std::unique_ptr<WidgetManager> wm;

        MenuBar *menuBar;
    };

} // namespace sky::editor
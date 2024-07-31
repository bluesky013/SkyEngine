//
// Created by blues on 2024/3/17.
//

#pragma once

#include <core/environment/Singleton.h>
#include <editor/WidgetManager.h>
#include <editor/widgets/Menu.h>
#include <editor/widgets/WorldWidget.h>
#include <editor/widgets/Guizmo.h>
#include <framework/interface/IGizmo.h>
#include <framework/world/World.h>
#include <memory>

namespace sky::editor {

    class EditorGuiInstance : public IGizmo {
    public:
        EditorGuiInstance() = default;
        ~EditorGuiInstance() override;

        void Init(World &world, NativeWindow* window);
    private:
        void Init();

        std::unique_ptr<WidgetManager> wm;
        std::unique_ptr<GuiZmoWidget> gui;
        ImGuiInstance* guiInstance = nullptr;
    };

} // namespace sky::editor
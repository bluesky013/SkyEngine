//
// Created by blues on 2024/3/17.
//

#pragma once

#include <core/environment/Singleton.h>
#include <editor/WidgetManager.h>
#include <editor/widgets/Menu.h>
#include <editor/widgets/WorldWidget.h>
#include <editor/widgets/Guizmo.h>
#include <framework/world/World.h>
#include <memory>

namespace sky::editor {

    class EditorInstance : public Singleton<EditorInstance>, public IWorldEvent {
    public:
        EditorInstance() = default;
        ~EditorInstance() override;

        void Init(ImGuiInstance *instance);

    private:
        // World event
        void OnCreateWorld(const WorldPtr& world) override;
        void OnDestroyWorld(const WorldPtr& world) override;

        std::unique_ptr<WidgetManager> wm;
        std::unique_ptr<GuiZmoWidget> gui;



        WorldWidget *worldWidget = nullptr;
        MenuBar *menuBar = nullptr;
    };

} // namespace sky::editor
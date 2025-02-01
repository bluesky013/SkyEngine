//
// Created by blues on 2024/11/28.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/name/Name.h>
#include <unordered_map>
#include <memory>

#include <QWidget>
#include <framework/window/IWindowEvent.h>

namespace sky {
    class World;
} // namespace sky

namespace sky::editor {
    class ToolWidget;
    class WorldTreeView;
    class EditorCamera;

    class ToolBase {
    public:
        ToolBase() = default;
        virtual ~ToolBase() = default;

        virtual bool Init() = 0;
        virtual void Shutdown() = 0;
        virtual ToolWidget *InitToolWidget(QWidget*) { return nullptr; }
    };

    class ToolWidget : public QWidget {
    public:
        explicit ToolWidget(QWidget* parent) : QWidget(parent) {}
        ~ToolWidget() override = default;

        virtual void Activate(WindowID winId) = 0;
        virtual void DeActivate() = 0;

        void SetWorld(WorldTreeView* world_)
        {
            world = world_;
            OnSetWorld();
        }

        void SetCamera(EditorCamera* camera_)
        {
            camera = camera_;
        }
    protected:
        virtual void OnSetWorld() {}

        WorldTreeView *world = nullptr;
        EditorCamera  *camera = nullptr;
    };

    class EditorToolManager : public Singleton<EditorToolManager> {
    public:
        EditorToolManager() = default;
        ~EditorToolManager() override = default;

        void RegisterTool(const Name &name, ToolBase* tool);
        void UnRegisterTool(const Name &name);

        const std::unordered_map<Name, std::unique_ptr<ToolBase>> &GetTools() const
        {
            return tools;
        }

    private:
        std::unordered_map<Name, std::unique_ptr<ToolBase>> tools;
    };

} // namespace::editor

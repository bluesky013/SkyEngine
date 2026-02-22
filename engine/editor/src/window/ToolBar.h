//
// Created by blues on 2024/11/29.
//

#pragma once

#include <QToolBar>
#include <QComboBox>
#include <QString>
#include <QMap>
#include <QMenu>
#include <QDockWidget>
#include <framework/interface/ISystem.h>
#include <framework/window/IWindowEvent.h>

namespace sky {
    class World;
} // namespace sky

namespace sky::editor {
    class ToolWidget;
    class WorldTreeView;
    class EditorCamera;

    class ToolBar : public QToolBar, public ISystemEvent {
    public:
        explicit ToolBar(QWidget *parent, QDockWidget* dockWidget);
        ~ToolBar() override = default;

        void SetWorld(WorldTreeView *world);
        void SetCamera(EditorCamera *camera);
    private:
        void OnMainWindowCreated(NativeWindow *window) override;

        void ResetToolWidget(const QString &name);
        void ResetAddActorMenu();

        QComboBox *comboBox = nullptr;
        QMenu *addMenu = nullptr;
        QDockWidget *toolDockWidget = nullptr;
        QMap<QString, ToolWidget*> toolMap;

        WindowID windowId;
        World* currentWorld = nullptr;
        EventBinder<ISystemEvent> binder;
    };


} // namespace sky::editor

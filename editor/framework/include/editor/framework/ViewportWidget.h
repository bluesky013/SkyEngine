//
// Created by blues on 2024/7/10.
//

#pragma once

#include <QWidget>
#include <QWindow>
#include <QBoxLayout>
#include <framework/window/NativeWindow.h>
#include <framework/world/World.h>
#include <framework/interface/ITickEvent.h>
#include <framework/interface/IGizmo.h>
#include <core/event/Event.h>
#include <render/debug/Grid.h>
#include <render/adaptor/RenderSceneProxy.h>
#include <render/adaptor/profile/RenderProfiler.h>
#include <editor/framework/EditorCamera.h>
#include <physics/PhysicsRegistry.h>
#include <physics/PhysicsWorld.h>

namespace sky {
    class RenderWindow;
} // namespace sky

namespace sky::editor {

    class ViewportWindow : public QWindow, public NativeWindow {
    public:
        explicit ViewportWindow(QWindow *parent = nullptr);
        ~ViewportWindow() override = default;

    private:
        bool event(QEvent *event) override;
    };

    class ViewportWidget : public QWidget, public IWindowEvent, public ITickEvent {
    public:
        explicit ViewportWidget(QWidget *parent);
        ~ViewportWidget() override;

        void ResetWorld(const WorldPtr &world);
        void UpdatePipeline();
        void ResetPipeline();

        ViewportWindow* GetViewportWindow() const { return window; }
    private:
        void OnWindowResize(uint32_t width, uint32_t height) override;
        void dropEvent(QDropEvent *event) override;
        void dragEnterEvent(QDragEnterEvent *event) override;
        void dragMoveEvent(QDragMoveEvent *event) override;

        void Tick(float time) override;

        ViewportWindow *window = nullptr;
        RenderWindow *renderWindow = nullptr;
        QWidget* windowContainer = nullptr;
        QVBoxLayout* layout = nullptr;
        WorldPtr world;

        RenderSceneProxy*  sceneProxy = nullptr;
        std::unique_ptr<EditorCamera> editorCamera;
        std::unique_ptr<IGizmo> gizmo;
        std::unique_ptr<Grid> grid;

        std::unique_ptr<RenderProfiler> profiler;
        EventBinder<ITickEvent> binder;
    };

} // namespace sky::editor
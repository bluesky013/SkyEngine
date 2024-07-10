//
// Created by Zach Lee on 2021/12/12.
//

#include <editor/framework/ViewportWidget.h>
#include <QPushButton>
#include <QVBoxLayout>
#include <render/adaptor/pipeline/DefaultForward.h>
#include <render/Renderer.h>

namespace sky::editor {

    ViewportWindow::ViewportWindow(QWindow *parent) : QWindow(parent)
    {
        setSurfaceType(SurfaceType::MetalSurface);

        winHandle = reinterpret_cast<void *>(winId());
        winID = winId();
    }

    bool ViewportWindow::event(QEvent *event)
    {
        switch (event->type()) {
            case QEvent::Resize:
                descriptor.width = width();
                descriptor.height = height();
                Event<IWindowEvent>::BroadCast(this, &IWindowEvent::OnWindowResize, descriptor.width, descriptor.height);
                break;
            case QEvent::MouseMove:
                break;
            case QEvent::Close:
                break;
            default: break;
        }
        return true;
    }

    ViewportWidget::ViewportWidget(QWidget *parent) : QWidget(parent)
    {
        layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
    }

    void ViewportWidget::ResetWorld(const WorldPtr &world_)
    {
        if (windowContainer != nullptr) {
            layout->removeWidget(windowContainer);
            delete windowContainer;
            windowContainer = nullptr;
        }

        world = world_;
        if (world != nullptr) {
            window = new ViewportWindow();
            windowContainer = QWidget::createWindowContainer(window, this, Qt::Widget);
            layout->addWidget(windowContainer);

            UpdatePipeline();
        } else {
            ResetPipeline();
        }
    }

    void ViewportWidget::ResetPipeline()
    {
        editorCamera->Shutdown();
        sceneProxy = nullptr;
        Renderer::Get()->DestroyRenderWindow(renderWindow);
        Event<IWindowEvent>::DisConnect(this);
    }

    void ViewportWidget::OnWindowResize(uint32_t width, uint32_t height)
    {
        if (renderWindow != nullptr) {
            renderWindow->Resize(width, height);
        }
    }

    void ViewportWidget::UpdatePipeline()
    {
        renderWindow = Renderer::Get()->CreateRenderWindow(window->GetNativeHandle(),
                                            window->GetWidth(),
                                            window->GetHeight(), false);

        auto *ppl = new DefaultForward();
        ppl->SetOutput(renderWindow);
        Renderer::Get()->SetPipeline(ppl);

        sceneProxy = std::make_unique<RenderSceneProxy>();
        world->AddSubSystem("RenderScene", sceneProxy.get());

        editorCamera = std::make_unique<EditorCamera>();
        editorCamera->Init(sceneProxy->GetRenderScene(), window);

        Event<IWindowEvent>::Connect(window, this);
    }
} // namespace sky::editor

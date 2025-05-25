//
// Created by Zach Lee on 2021/12/12.
//

#include <editor/framework/ViewportWidget.h>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDropEvent>
#include <QMimeData>
#include <QApplication>
#include <QScreen>
#include <render/adaptor/pipeline/DefaultForwardPipeline.h>
#include <render/adaptor/assets/TechniqueAsset.h>
#include <render/RenderPassPipeline.h>
#include <render/Renderer.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/asset/AssetManager.h>
#include <framework/window/NativeWindowManager.h>

namespace sky::editor {

    std::unordered_map<Qt::Key, ScanCode> SCANCODE_MAP = {
            {Qt::Key_A, ScanCode::KEY_A},
            {Qt::Key_B, ScanCode::KEY_B},
            {Qt::Key_C, ScanCode::KEY_C},
            {Qt::Key_D, ScanCode::KEY_D},
            {Qt::Key_E, ScanCode::KEY_E},
            {Qt::Key_F, ScanCode::KEY_F},
            {Qt::Key_G, ScanCode::KEY_G},
            {Qt::Key_H, ScanCode::KEY_H},
            {Qt::Key_I, ScanCode::KEY_I},
            {Qt::Key_J, ScanCode::KEY_J},
            {Qt::Key_K, ScanCode::KEY_K},
            {Qt::Key_L, ScanCode::KEY_L},
            {Qt::Key_M, ScanCode::KEY_M},
            {Qt::Key_N, ScanCode::KEY_N},
            {Qt::Key_O, ScanCode::KEY_O},
            {Qt::Key_P, ScanCode::KEY_P},
            {Qt::Key_Q, ScanCode::KEY_Q},
            {Qt::Key_R, ScanCode::KEY_R},
            {Qt::Key_S, ScanCode::KEY_S},
            {Qt::Key_T, ScanCode::KEY_T},
            {Qt::Key_U, ScanCode::KEY_U},
            {Qt::Key_V, ScanCode::KEY_V},
            {Qt::Key_W, ScanCode::KEY_W},
            {Qt::Key_X, ScanCode::KEY_X},
            {Qt::Key_Y, ScanCode::KEY_Y},
            {Qt::Key_Z, ScanCode::KEY_Z},
            {Qt::Key_1, ScanCode::KEY_1},
            {Qt::Key_2, ScanCode::KEY_2},
            {Qt::Key_3, ScanCode::KEY_3},
            {Qt::Key_4, ScanCode::KEY_4},
            {Qt::Key_5, ScanCode::KEY_5},
            {Qt::Key_6, ScanCode::KEY_6},
            {Qt::Key_7, ScanCode::KEY_7},
            {Qt::Key_8, ScanCode::KEY_8},
            {Qt::Key_9, ScanCode::KEY_9},
            {Qt::Key_0, ScanCode::KEY_0},
            {Qt::Key_Return, ScanCode::KEY_RETURN},
            {Qt::Key_Escape, ScanCode::KEY_ESCAPE},
            {Qt::Key_Backspace, ScanCode::KEY_BACKSPACE},
            {Qt::Key_Tab, ScanCode::KEY_TAB},
            {Qt::Key_Space, ScanCode::KEY_SPACE},
            {Qt::Key_Minus, ScanCode::KEY_MINUS},
            {Qt::Key_Equal, ScanCode::KEY_EQUALS},
//            {Qt::Key_, ScanCode::KEY_LEFTBRACKET},
//            {Qt::Key_, ScanCode::KEY_RIGHTBRACKET},
            {Qt::Key_Backslash, ScanCode::KEY_BACKSLASH},
//            {Qt::Key_, ScanCode::KEY_NONUSHASH},
            {Qt::Key_Semicolon, ScanCode::KEY_SEMICOLON},
            {Qt::Key_Apostrophe, ScanCode::KEY_APOSTROPHE},
//            {Qt::Key_, ScanCode::KEY_GRAVE},
            {Qt::Key_Comma, ScanCode::KEY_COMMA},
            {Qt::Key_Period, ScanCode::KEY_PERIOD},
            {Qt::Key_Slash, ScanCode::KEY_SLASH},
            {Qt::Key_CapsLock, ScanCode::KEY_CAPSLOCK},
            {Qt::Key_F1, ScanCode::KEY_F1},
            {Qt::Key_F2, ScanCode::KEY_F2},
            {Qt::Key_F3, ScanCode::KEY_F3},
            {Qt::Key_F4, ScanCode::KEY_F4},
            {Qt::Key_F5, ScanCode::KEY_F5},
            {Qt::Key_F6, ScanCode::KEY_F6},
            {Qt::Key_F7, ScanCode::KEY_F7},
            {Qt::Key_F8, ScanCode::KEY_F8},
            {Qt::Key_F9, ScanCode::KEY_F9},
            {Qt::Key_F10, ScanCode::KEY_F10},
            {Qt::Key_F11, ScanCode::KEY_F11},
            {Qt::Key_F12, ScanCode::KEY_F12},
            {Qt::Key_Print, ScanCode::KEY_PRINTSCREEN},
            {Qt::Key_ScrollLock, ScanCode::KEY_SCROLLLOCK},
            {Qt::Key_Pause, ScanCode::KEY_PAUSE},
            {Qt::Key_Insert, ScanCode::KEY_INSERT},
            {Qt::Key_Home, ScanCode::KEY_HOME},
            {Qt::Key_PageUp, ScanCode::KEY_PAGEUP},
            {Qt::Key_Delete, ScanCode::KEY_DELETE},
            {Qt::Key_End, ScanCode::KEY_END},
            {Qt::Key_PageDown, ScanCode::KEY_PAGEDOWN},
            {Qt::Key_Right, ScanCode::KEY_RIGHT},
            {Qt::Key_Left, ScanCode::KEY_LEFT},
            {Qt::Key_Down, ScanCode::KEY_DOWN},
            {Qt::Key_Up, ScanCode::KEY_UP},
            {Qt::Key_NumLock, ScanCode::KEY_NUMLOCKCLEAR},
            {Qt::Key_division, ScanCode::KEY_KP_DIVIDE},
            {Qt::Key_multiply, ScanCode::KEY_KP_MULTIPLY},
            {Qt::Key_Minus, ScanCode::KEY_KP_MINUS},
            {Qt::Key_Plus, ScanCode::KEY_KP_PLUS},
            {Qt::Key_Enter, ScanCode::KEY_KP_ENTER},
//            {Qt::Key_, ScanCode::KEY_KP_1},
//            {Qt::Key_, ScanCode::KEY_KP_2},
//            {Qt::Key_, ScanCode::KEY_KP_3},
//            {Qt::Key_, ScanCode::KEY_KP_4},
//            {Qt::Key_, ScanCode::KEY_KP_5},
//            {Qt::Key_, ScanCode::KEY_KP_6},
//            {Qt::Key_, ScanCode::KEY_KP_7},
//            {Qt::Key_, ScanCode::KEY_KP_8},
//            {Qt::Key_, ScanCode::KEY_KP_9},
//            {Qt::Key_, ScanCode::KEY_KP_0},
//            {Qt::Key_, ScanCode::KEY_KP_PERIOD},
    };

    static KeyModFlags ConvertModifier(Qt::KeyboardModifiers modifier)
    {
        KeyModFlags flags = {};
        if ((modifier & Qt::SHIFT) != 0) { flags |= KeyMod::SHIFT; }
        if ((modifier & Qt::CTRL) != 0) { flags |= KeyMod::CTRL; }
        if ((modifier & Qt::ALT) != 0) { flags |= KeyMod::ALT; }

        return flags;
    }

    ViewportWindow::ViewportWindow(QWindow *parent) : QWindow(parent)
    {
        winHandle = GetNativeWindow();
        winID = reinterpret_cast<WindowID>(winHandle);

        QScreen *screen = QApplication::primaryScreen();
        scale = screen->devicePixelRatio();

        NativeWindowManager::Get()->Register(this);
    }

    ViewportWidget::~ViewportWidget()
    {
        ResetPipeline();
    }

    bool ViewportWindow::event(QEvent *event)
    {
        switch (event->type()) {
            case QEvent::Resize: {
                descriptor.width = (uint32_t)(width() * scale);
                descriptor.height = (uint32_t)(height() * scale);
                WindowResizeEvent wEvent = {};
                wEvent.winID = winID;
                wEvent.width = descriptor.width;
                wEvent.height = descriptor.height;
                Event<IWindowEvent>::BroadCast(this, &IWindowEvent::OnWindowResize, wEvent);
                break;
            }
            case QEvent::MouseMove: {
                auto* mouseEvent = static_cast<QMouseEvent*>(event);
                auto pos = mouseEvent->pos();
                auto globalPos = mouseEvent->globalPos();
                MouseMotionEvent mEvent = {};
                mEvent.winID = winID;
                mEvent.x = pos.x();
                mEvent.y = pos.y();
                mEvent.relX = globalPos.x();
                mEvent.relY = globalPos.y();
                Event<IMouseEvent>::BroadCast(&IMouseEvent::OnMouseMotion, mEvent);
                break;
            }
            case QEvent::MouseButtonPress:
            case QEvent::MouseButtonRelease: {
                auto* mouseEvent = static_cast<QMouseEvent*>(event);
                auto pressPos = mouseEvent->globalPos();
                auto button = mouseEvent->button();

                MouseButtonEvent mEvent = {};
                mEvent.winID = winID;
                mEvent.x = pressPos.x();
                mEvent.y = pressPos.y();
                if (button == Qt::LeftButton) { mEvent.button = MouseButtonType::LEFT; }
                else if (button == Qt::RightButton) { mEvent.button = MouseButtonType::RIGHT; }
                else if (button == Qt::MiddleButton) { mEvent.button = MouseButtonType::MIDDLE; }

                if (event->type() == QEvent::MouseButtonRelease) {
                    Event<IMouseEvent>::BroadCast(&IMouseEvent::OnMouseButtonUp, mEvent);
                } else {
                    Event<IMouseEvent>::BroadCast(&IMouseEvent::OnMouseButtonDown, mEvent);
                }
                break;
            }
            case QEvent::Wheel: {
                auto* wheelEvent = static_cast<QWheelEvent*>(event);
                auto point = wheelEvent->angleDelta() / 20.f;

                MouseWheelEvent mEvent = {};
                mEvent.winID = winID;
                mEvent.x = static_cast<int32_t>(point.x());
                mEvent.y = static_cast<int32_t>(point.y());
                Event<IMouseEvent>::BroadCast(&IMouseEvent::OnMouseWheel, mEvent);
                break;
            }
            case QEvent::KeyPress:
            case QEvent::KeyRelease: {
                auto* keyEvent = static_cast<QKeyEvent*>(event);
                auto scanCode = static_cast<Qt::Key>(keyEvent->key());
                auto mod = ConvertModifier(keyEvent->modifiers());

                auto iter = SCANCODE_MAP.find(scanCode);
                if (iter == SCANCODE_MAP.end()) {
                    break;
                }

                KeyboardEvent kEvent = {};
                kEvent.winID = winID;
                kEvent.scanCode = iter->second;
                kEvent.mod = mod;
                if (event->type() == QEvent::KeyRelease) {
                    Event<IKeyboardEvent>::BroadCast(&IKeyboardEvent::OnKeyUp, kEvent);
                } else {
                    Event<IKeyboardEvent>::BroadCast(&IKeyboardEvent::OnKeyDown, kEvent);
                }
                break;
            }
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

        grid = std::make_unique<Grid>();

        binder.Bind(this);
        setAcceptDrops(true);
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
            Event<ISystemEvent>::BroadCast(&ISystemEvent::OnMainWindowCreated, window);

            UpdatePipeline();
        } else {

            ResetPipeline();
        }
    }

    void ViewportWidget::ResetPipeline()
    {
        if (editorCamera) {
            editorCamera->Shutdown();
        }
        Renderer::Get()->SetPipeline(nullptr);
        profiler = nullptr;
        gizmo = nullptr;
        sceneProxy = nullptr;
        Renderer::Get()->DestroyRenderWindow(renderWindow);
        Event<IWindowEvent>::DisConnect(this);
    }

    void ViewportWidget::Tick(float time)
    {
        if (editorCamera) {
            editorCamera->Tick(time);
        }

        if (profiler) {
            profiler->Tick();
        }
    }

    void ViewportWidget::OnWindowResize(const WindowResizeEvent& event)
    {
        if (renderWindow != nullptr) {
            renderWindow->Resize(event.width, event.height);
            editorCamera->UpdateAspect(event.width, event.height);
        }

        if (profiler) {
            profiler->SetDisplaySize(event.width, event.height);
        }
    }

    void ViewportWidget::dropEvent(QDropEvent *event)
    {
        const auto *mimeData = event->mimeData();
        if (mimeData->hasUrls()) {
            auto urls = mimeData->urls();
        }
    }

    void ViewportWidget::dragEnterEvent(QDragEnterEvent *event)
    {
    }

    void ViewportWidget::dragMoveEvent(QDragMoveEvent *event)
    {
    }

    void ViewportWidget::UpdatePipeline()
    {
        renderWindow = Renderer::Get()->CreateRenderWindow(window->GetNativeHandle(),
                                            window->GetWidth(),
                                            window->GetHeight(), false);

//        auto *ppl = new DefaultForward();
        auto *ppl = new RenderPassPipeline();
        Renderer::Get()->SetPipeline(ppl);

        sceneProxy = static_cast<RenderSceneProxy*>(world->GetSubSystem(Name("RenderScene")));
        auto *scenePipeline = new DefaultForwardPipeline(sceneProxy->GetRenderScene());
        scenePipeline->SetOutput(renderWindow);
        ppl->AddScenePass(scenePipeline);

        auto debugTech = AssetManager::Get()->LoadAssetFromPath<Technique>("techniques/debug.tech");
        debugTech->BlockUntilLoaded();
        grid->SetTechnique(CreateTechniqueFromAsset(debugTech));
        grid->Draw(200.f);
//        sceneProxy->GetRenderScene()->AddPrimitive(grid->GetPrimitive());

        editorCamera = std::make_unique<EditorCamera>();
        editorCamera->Init(sceneProxy->GetRenderScene(), window);

        auto *gizmoFactory = Interface<IGizmoFactory>::Get()->GetApi();
        if (gizmoFactory != nullptr) {
            gizmo.reset(gizmoFactory->CreateGizmo());
            gizmo->Init(*world, window);
        }

        profiler = std::make_unique<RenderProfiler>(sceneProxy->GetRenderScene());
        profiler->SetDisplaySize(window->GetWidth(), window->GetHeight());

        Event<IWindowEvent>::Connect(window, this);
    }
} // namespace sky::editor

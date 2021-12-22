//
// Created by Zach Lee on 2021/12/12.
//

#include "MainWindow.h"
#include <QTimer>
#include <QDockWidget>
#include <engine/SkyEngine.h>
#include <engine/world/World.h>
#include <engine/world/GameObject.h>
#include <engine/render/camera/CameraComponent.h>
#include <engine/render/light/LightComponent.h>
#include <editor/dockwidget/WorldWidget.h>
#include <editor/inspector/InspectorWidget.h>
#include <editor/dockwidget/DockManager.h>
#include "CentralWidget.h"

namespace sky::editor {

    MainWindow::MainWindow(QWidget *parent)
        : QMainWindow(parent)
        , engine(nullptr)
        , world(nullptr)
        , timer(nullptr)
    {
        Init();
    }

    MainWindow::~MainWindow() {}

    void MainWindow::InitWorld()
    {
        engine = SkyEngine::Get();
        world = new World();
        engine->AddWorld(*world);

        auto camera = world->CreateGameObject("camera");
        camera->AddComponent<CameraComponent>();

        auto light = world->CreateGameObject("light");
        light->AddComponent<LightComponent>();
    }

    void MainWindow::ShutdownWorld()
    {
        engine->RemoveWorld(*world);
        delete world;
        world = nullptr;
        engine = nullptr;
    }

    void MainWindow::OnTick()
    {
        static auto timePoint = std::chrono::high_resolution_clock::now();
        auto current = std::chrono::high_resolution_clock::now();
        auto delta = std::chrono::duration<float>(current - timePoint).count();
        timePoint = current;
        if (engine != nullptr) {
            engine->Tick(delta);
        }
    }

    void MainWindow::Init()
    {
        InitWorld();

        setWindowState(Qt::WindowMaximized);

        auto centralWidget = new CentralWidget(this);
        setCentralWidget(centralWidget);
        centralWidget->Init();
        auto vp = centralWidget->GetViewport();
        if (vp != nullptr) {
            viewports.emplace_back(vp);
        }
        auto dockMgr = DockManager::Get();

        auto worldWidget = new WorldWidget(this);
        worldWidget->SetWorld(*world);
        addDockWidget(Qt::DockWidgetArea::LeftDockWidgetArea, worldWidget);
        dockMgr->Register((uint32_t)DockId::WORLD, *worldWidget);

        auto inspector = new InspectorWidget(this);
        addDockWidget(Qt::DockWidgetArea::RightDockWidgetArea, inspector);
        dockMgr->Register((uint32_t)DockId::INSPECTOR, *inspector);

        addDockWidget(Qt::DockWidgetArea::BottomDockWidgetArea, new QDockWidget(this));

        timer = new QTimer(this);
        timer->start(0);
        connect(timer, &QTimer::timeout, this, &MainWindow::OnTick);
    }

    void MainWindow::Shutdown()
    {
        for (auto& vp : viewports) {
            vp->Shutdown();
        }
    }

}
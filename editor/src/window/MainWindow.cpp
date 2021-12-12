//
// Created by Zach Lee on 2021/12/12.
//

#include "MainWindow.h"
#include <QTimer>
#include <QDockWidget>
#include <engine/SkyEngine.h>
#include <engine/world/World.h>

#include <editor/viewport/ViewportWidget.h>
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
        setSizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Preferred);

        auto centralWidget = new CentralWidget(this);
        setCentralWidget(centralWidget);
        auto vp = centralWidget->GetViewport();
        if (vp != nullptr) {
            viewports.emplace_back(vp);
        }

        addDockWidget(Qt::DockWidgetArea::LeftDockWidgetArea, new QDockWidget(this));
        addDockWidget(Qt::DockWidgetArea::RightDockWidgetArea, new QDockWidget(this));
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
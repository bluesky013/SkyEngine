//
// Created by Zach Lee on 2021/12/12.
//

#include "MainWindow.h"
#include "ActionManager.h"
#include "CentralWidget.h"
#include "Constants.h"
#include <QDockWidget>
#include <QFileDialog>
#include <QMenuBar>
#include <QTimer>
#include <editor/dockwidget/DockManager.h>
#include <editor/dockwidget/WorldWidget.h>
#include <editor/inspector/InspectorWidget.h>
#include <engine/SkyEngine.h>
#include <engine/world/GameObject.h>

namespace sky::editor {

    MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), engine(nullptr), timer(nullptr)
    {
        Init();
        InitMenu();
    }

    MainWindow::~MainWindow()
    {
        if (actionManager != nullptr) {
            delete actionManager;
            actionManager = nullptr;
        }
    }

    void MainWindow::InitEngine()
    {
        engine = SkyEngine::Get();
    }

    void MainWindow::ShutdownEngine()
    {
        engine->DeInit();
        SkyEngine::Destroy();
        engine = nullptr;
    }

    void MainWindow::OnTick()
    {
        static auto timePoint = std::chrono::high_resolution_clock::now();
        auto        current   = std::chrono::high_resolution_clock::now();
        auto        delta     = std::chrono::duration<float>(current - timePoint).count();
        timePoint             = current;
        if (engine != nullptr) {
            engine->Tick(delta);
        }
    }

    void MainWindow::OnOpenProject(const QString &path)
    {
        actionManager->Update(1 << PROJECT_OPEN_BIT);
    }

    void MainWindow::Init()
    {
        InitEngine();

        setWindowState(Qt::WindowMaximized);

        auto centralWidget = new CentralWidget(this);
        setCentralWidget(centralWidget);
        centralWidget->Init();
        auto vp = centralWidget->GetViewport();
        if (vp != nullptr) {
            viewports.emplace_back(vp);
        }

        auto dockMgr     = DockManager::Get();
        auto worldWidget = new WorldWidget(this);
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

    void MainWindow::InitMenu()
    {
        actionManager = new ActionManager();

        menuBar = new QMenuBar(this);

        ActionWithFlag *openLevelAct = new ActionWithFlag(1 << PROJECT_OPEN_BIT, "Open Level");

        ActionWithFlag *closeLevelAct = new ActionWithFlag(1 << LEVEL_OPEN_BIT, "Close Level");

        ActionWithFlag *newLevelAct = new ActionWithFlag(1 << PROJECT_OPEN_BIT, "New Level");

        ActionWithFlag *openProjectAct = new ActionWithFlag(0, "Open Project", this);
        connect(openProjectAct, &QAction::triggered, this, [this](bool /**/) {
            QFileDialog dialog(this);
            dialog.setFileMode(QFileDialog::AnyFile);
            dialog.setNameFilter(tr("Projects (*.sproj)"));
            dialog.setViewMode(QFileDialog::Detail);
            QStringList fileNames;
            if (dialog.exec()) {
                fileNames = dialog.selectedFiles();
                if (!fileNames.empty()) {
                    OnOpenProject(fileNames[0]);
                }
            }
        });

        ActionWithFlag *closeAct = new ActionWithFlag(0, "Close", this);
        connect(closeAct, &QAction::triggered, this, [this](bool /**/) { close(); });

        auto fileMenu = new QMenu("File", menuBar);
        fileMenu->addAction(openProjectAct);
        fileMenu->addSeparator();
        fileMenu->addAction(newLevelAct);
        fileMenu->addAction(openLevelAct);
        fileMenu->addAction(closeLevelAct);
        fileMenu->addSeparator();
        fileMenu->addAction(closeAct);
        menuBar->addMenu(fileMenu);

        setMenuBar(menuBar);

        actionManager->AddAction(openLevelAct);
        actionManager->AddAction(closeLevelAct);
        actionManager->AddAction(newLevelAct);
        actionManager->AddAction(openProjectAct);
        actionManager->AddAction(closeAct);
        actionManager->Update(0);
    }
} // namespace sky::editor
//
// Created by Zach Lee on 2021/12/12.
//

#include "MainWindow.h"
#include "ActionManager.h"
#include "CentralWidget.h"
#include "../dialog/ProjectDialog.h"
#include <QDockWidget>
#include <QFileDialog>
#include <QMenuBar>
#include <QTimer>
#include <QMessageBox>
#include <editor/dockwidget/DockManager.h>
#include <editor/dockwidget/WorldWidget.h>
#include <editor/dockwidget/AssetWidget.h>
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
        document = std::make_unique<Document>(path);
        document->Init();

        QFileInfo file(path);
        auto dir = file.path().toStdString();
        assetBrowser->OnProjectChange(file.path());
        UpdateActions();
    }

    void MainWindow::OnNewProject(const QString &path, const QString &name)
    {
        QDir dir(path);
        QDir fullPath(path + QDir::separator() + name);
        QDir fullPathFile(path + QDir::separator() + name + QDir::separator() + name + tr(".sproj"));

        if (dir.exists()){
            QDir check(path);
            if (fullPath.exists()) {
                QMessageBox msgWarning;
                msgWarning.setText("Error!\nDirectory Exists.");
                msgWarning.setIcon(QMessageBox::Critical);
                msgWarning.setWindowTitle("Caution");
                msgWarning.exec();
                return;
            }

            dir.mkdir(name);
        }
        OnOpenProject(fullPathFile.path());
    }

    void MainWindow::OnCloseProject()
    {
        document.reset();
        UpdateActions();
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
        worldWidget = new WorldWidget(this);
        addDockWidget(Qt::DockWidgetArea::LeftDockWidgetArea, worldWidget);
        dockMgr->Register((uint32_t)DockId::WORLD, *worldWidget);

        inspector = new InspectorWidget(this);
        addDockWidget(Qt::DockWidgetArea::RightDockWidgetArea, inspector);
        dockMgr->Register((uint32_t)DockId::INSPECTOR, *inspector);

        assetBrowser = new AssetWidget(this);
        addDockWidget(Qt::DockWidgetArea::BottomDockWidgetArea, assetBrowser);
        dockMgr->Register((uint32_t)DockId::BROWSER, *assetBrowser);

        timer = new QTimer(this);
        timer->start(0);
        connect(timer, &QTimer::timeout, this, &MainWindow::OnTick);
    }

    void MainWindow::InitMenu()
    {
        actionManager = new ActionManager();

        menuBar = new QMenuBar(this);

        ActionWithFlag *openLevelAct = new ActionWithFlag(DocumentFlagBit::PROJECT_OPEN, "Open Level");

        ActionWithFlag *closeLevelAct = new ActionWithFlag(DocumentFlagBit::LEVEL_OPEN, "Close Level");

        ActionWithFlag *newLevelAct = new ActionWithFlag(DocumentFlagBit::PROJECT_OPEN, "New Level");

        ActionWithFlag *openProjectAct = new ActionWithFlag({}, "Open Project", this);

        ActionWithFlag *newProjectAct = new ActionWithFlag({}, "New Project", this);

        ActionWithFlag *closeProjectAct = new ActionWithFlag({}, "Close Project", this);

        // project
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

        connect(newProjectAct, &QAction::triggered, this, [this](bool /**/) {
            ProjectDialog dialog;
            if (dialog.exec()) {
                auto projectPath = dialog.ProjectPath();
                auto projectName = dialog.ProjectName();
                if (!projectPath.isEmpty() && !projectName.isEmpty()) {
                    OnNewProject(projectPath, projectName);
                }
            }
        });

        connect(closeProjectAct, &QAction::triggered, this, [this](bool /**/) {
            OnCloseProject();
        });

        // close editor
        ActionWithFlag *closeAct = new ActionWithFlag({}, "Close", this);
        connect(closeAct, &QAction::triggered, this, [this](bool /**/) { close(); });

        auto fileMenu = new QMenu("File", menuBar);
        fileMenu->addAction(newProjectAct);
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
        UpdateActions();
    }

    void MainWindow::UpdateActions()
    {
        actionManager->Update(document ? document->GetFlag() : DocFlagArray{});
    }
} // namespace sky::editor
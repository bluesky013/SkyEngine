//
// Created by Zach Lee on 2021/12/12.
//

#include <editor/window/MainWindow.h>
#include "ActionManager.h"
#include "CentralWidget.h"
#include "ToolBar.h"
#include <QDockWidget>
#include <QFileDialog>
#include <QMenuBar>
#include <QTimer>
#include <QMessageBox>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetBuilderManager.h>
#include <framework/interface/IWorldBuilder.h>
#include <editor/framework/ViewportWidget.h>
#include <editor/dockwidget/DockManager.h>
#include <editor/dockwidget/WorldWidget.h>
#include <editor/dockwidget/AssetWidget.h>
#include <editor/dockwidget/InspectorWidget.h>

namespace sky::editor {

    MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
    {
        InitDocument();
        InitWidgets();
        InitMenu();
        UpdateActions();
    }

    MainWindow::~MainWindow()
    {
        ActionManager::Destroy();
    }

    void MainWindow::OnOpenWorld()
    {
        QFileDialog dialog(this);
        dialog.setFileMode(QFileDialog::AnyFile);
        dialog.setNameFilter(tr("World (*.world)"));
        dialog.setViewMode(QFileDialog::Detail);
        dialog.setDirectory(projectPath);
        QStringList fileNames;
        if (dialog.exec() != 0) {
            fileNames = dialog.selectedFiles();
        }
        if (fileNames.empty()) {
            return;
        }

        auto world = document->OpenWorld(fileNames[0]);
        if (!world) {
            return;
        }

        worldWidget->SetWorld(world);
        mainViewport->ResetWorld(world);
        toolBar->SetWorld(worldWidget->GetWorldTreeView());
        toolBar->SetCamera(mainViewport->GetCamera());
        setCentralWidget(mainViewport);
        UpdateActions();
    }

    void MainWindow::OnNewWorld()
    {
        QString fineName = QFileDialog::getSaveFileName(nullptr, "Create", projectPath, tr("World (*.world)"));
        if (fineName.isEmpty()) {
            return;
        }

        auto world = document->OpenWorld(fineName);
        if (!world) {
            return;
        }

        worldWidget->SetWorld(world);
        mainViewport->ResetWorld(world);
        toolBar->SetWorld(worldWidget->GetWorldTreeView());
        toolBar->SetCamera(mainViewport->GetCamera());
        UpdateActions();
    }

    void MainWindow::OnCloseWorld()
    {
        SaveCheck();
        worldWidget->SetWorld(nullptr);
        toolBar->SetWorld(nullptr);
        toolBar->SetCamera(nullptr);
        inspector->OnSelectedItemChanged(nullptr);
        mainViewport->ResetWorld(nullptr);
        document->CloseWorld();
        UpdateActions();
    }

    void MainWindow::OnSaveWorld()
    {
        document->SaveWorld();
    }

    void MainWindow::SaveCheck()
    {
        if (!document->NeedSave()) {
            return;
        }

        QMessageBox msgBox;
        msgBox.setText("The document has been modified.");
        msgBox.setInformativeText("Do you want to save your changes?");
        msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Save);
        int ret = msgBox.exec();

        switch (ret) {
            case QMessageBox::Save:
                OnSaveWorld();
                break;
            default:
                break;
        }
    }

    void MainWindow::OnImport()
    {
        QFileDialog dialog(this);
        dialog.setFileMode(QFileDialog::AnyFile);
        dialog.setViewMode(QFileDialog::Detail);
        dialog.setDirectory(projectPath);
        QStringList fileNames;
        if (dialog.exec() != 0) {
            fileNames = dialog.selectedFiles();
        }
        if (fileNames.empty()) {
            return;
        }
        AssetBuilderManager::Get()->ImportAsset({fileNames[0].toStdString()});
    }

    void MainWindow::InitWidgets()
    {
        setWindowState(Qt::WindowMaximized);

        emptyCentral = new CentralWidget(this);
        setCentralWidget(emptyCentral);

        auto *dockMgr = DockManager::Get();
        worldWidget = new WorldWidget(this);
        addDockWidget(Qt::DockWidgetArea::LeftDockWidgetArea, worldWidget);
        dockMgr->Register((uint32_t)DockId::WORLD, *worldWidget);

        inspector = new InspectorWidget(this);
        addDockWidget(Qt::DockWidgetArea::RightDockWidgetArea, inspector);
        dockMgr->Register((uint32_t)DockId::INSPECTOR, *inspector);

        connect(worldWidget->GetWorldTreeView(), &WorldTreeView::WorldTreeSelectItemChanged, inspector, &InspectorWidget::OnSelectedItemChanged);

        assetBrowser = new AssetWidget(this);
        addDockWidget(Qt::DockWidgetArea::BottomDockWidgetArea, assetBrowser);
        dockMgr->Register((uint32_t)DockId::BROWSER, *assetBrowser);

        toolWidget = new QDockWidget(this);
        addDockWidget(Qt::DockWidgetArea::LeftDockWidgetArea, toolWidget);
        dockMgr->Register((uint32_t)DockId::TOOL, *toolWidget);

        mainViewport = new ViewportWidget(nullptr);
        setCentralWidget(mainViewport);

        projectPath = AssetDataBase::Get()->GetWorkSpaceFs()->GetPath().GetStr().c_str();
    }

    void MainWindow::InitDocument()
    {
        document = std::make_unique<Document>();
    }

    void MainWindow::InitMenu()
    {
        actionManager = ActionManager::Get();

        menuBar = new QMenuBar(this);

        // level
        auto *openWorldAct = new ActionWithFlag(DocumentFlagBit::ProjectOpen, "Open World");
        auto *closeWorldAct = new ActionWithFlag(DocumentFlagBit::WorldOpen, "Close World");
        auto *newWorldAct = new ActionWithFlag(DocumentFlagBit::ProjectOpen, "New World");
        auto *saveWorldAct = new ActionWithFlag(DocumentFlagBit::WorldOpen, "Save World");

        connect(openWorldAct, &QAction::triggered, this, [this]() { OnOpenWorld(); });
        connect(newWorldAct, &QAction::triggered, this, [this]() { OnNewWorld(); });
        connect(closeWorldAct, &QAction::triggered, this, [this]() { OnCloseWorld(); });
        connect(saveWorldAct, &QAction::triggered, this, [this]() { OnSaveWorld(); });

        // import
        auto *importAct = new ActionWithFlag(DocumentFlagBit::ProjectOpen, "Import Asset");
        connect(importAct, &QAction::triggered, this, [this]() { OnImport(); });

        // close editor
        auto *closeAct = new ActionWithFlag({}, "Close", this);
        connect(closeAct, &QAction::triggered, this, [this](bool /**/) { close(); });

        auto *fileMenu = new QMenu("File", menuBar);
        fileMenu->addAction(newWorldAct);
        fileMenu->addAction(openWorldAct);
        fileMenu->addAction(saveWorldAct);
        fileMenu->addAction(closeWorldAct);
        fileMenu->addSeparator();
        fileMenu->addAction(importAct);
        fileMenu->addSeparator();
        fileMenu->addAction(closeAct);

        // world builders
        auto *toolMenu = new QMenu("Tools", menuBar);

        auto *buildMenu = new QMenu("Build", toolMenu);
        toolMenu->addMenu(buildMenu);

        std::list<CounterPtr<IWorldBuilder>> builders;
        Event<IWorldBuilderGather>::BroadCast(&IWorldBuilderGather::Gather, builders);
        for (auto &builder : builders) {
            auto *act = new ActionWithFlag(DocumentFlagBit::WorldOpen, builder->GetDesc().c_str(), this);
            connect(act, &QAction::triggered, this, [this, builder](bool /**/) {
                builder->Build(document->GetWorld());
            });

            buildMenu->addAction(act);
        }

        menuBar->addMenu(fileMenu);
        menuBar->addMenu(toolMenu);

        setMenuBar(menuBar);

        // toolbar
        toolBar = new ToolBar(this, toolWidget);
        addToolBar(Qt::TopToolBarArea, toolBar);

        actionManager->AddAction(newWorldAct);
        actionManager->AddAction(openWorldAct);
        actionManager->AddAction(closeWorldAct);
        actionManager->AddAction(closeAct);
        UpdateActions();
    }

    void MainWindow::UpdateActions()
    {
        actionManager->Update(document ? document->GetFlag() : DocFlagArray{});
    }

    bool MainWindow::event(QEvent *event)
    {
        switch (event->type()) {
            case QEvent::Close:
                OnCloseWorld();
                break;
            default:
                break;
        }
        return QMainWindow::event(event);
    }

    ViewportWindow* MainWindow::GetViewportWindow() const
    {
        return mainViewport->GetViewportWindow();
    }

} // namespace sky::editor

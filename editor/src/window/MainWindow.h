
//
// Created by Zach Lee on 2021/12/12.
//

#pragma once
#include <QMainWindow>

class QDockWidget;
class QMenuBar;
class QTimer;

namespace sky {
    class SkyEngine;
}

namespace sky::editor {

    class ActionManager;
    class ViewportWidget;

    class MainWindow : public QMainWindow
    {
        Q_OBJECT
    public:
        MainWindow(QWidget *parent = nullptr);
        ~MainWindow();

        void Init();
    private:
        void InitEngine();

        void InitMenu();

        void ShutdownEngine();

        void OnTick();

        void OnOpenProject(const QString& path);

        SkyEngine* engine = nullptr;
        QTimer* timer = nullptr;
        QMenuBar* menuBar = nullptr;
        ActionManager* actionManager = nullptr;
        std::vector<ViewportWidget*> viewports;
        std::list<QDockWidget*> docks;
    };

}
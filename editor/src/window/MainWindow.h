
//
// Created by Zach Lee on 2021/12/12.
//

#pragma once
#include <QMainWindow>
#include <QTimer>
#include <engine/world/World.h>

class QDockWidget;

namespace sky {
    class SkyEngine;
}

namespace sky::editor {

    class ViewportWidget;

    class MainWindow : public QMainWindow
    {
        Q_OBJECT
    public:
        MainWindow(QWidget *parent = nullptr);
        ~MainWindow();

        void Init();

        void Shutdown();

    private:
        void InitWorld();

        void ShutdownWorld();

        void OnTick();

        SkyEngine* engine;
        World* world;
        QTimer* timer;
        std::vector<ViewportWidget*> viewports;
        std::list<QDockWidget*> docks;
    };

}
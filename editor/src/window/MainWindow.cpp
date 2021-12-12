//
// Created by Zach Lee on 2021/12/12.
//

#include "MainWindow.h"
#include <QTimer>
#include <QDockWidget>

namespace sky::editor {

    MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {}

    MainWindow::~MainWindow() {}

    void MainWindow::Init()
    {
        setSizePolicy(QSizePolicy::Policy::Maximum, QSizePolicy::Policy::Maximum);

        addDockWidget(Qt::DockWidgetArea::LeftDockWidgetArea, new QDockWidget(this));
        addDockWidget(Qt::DockWidgetArea::RightDockWidgetArea, new QDockWidget(this));
        addDockWidget(Qt::DockWidgetArea::BottomDockWidgetArea, new QDockWidget(this));


        QTimer* timer = new QTimer(this);
        timer->start(0);
//        connect(timer, &QTimer::timeout, this, &MainWindow::onDraw);
    }

}
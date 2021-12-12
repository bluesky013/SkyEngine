
//
// Created by Zach Lee on 2021/12/12.
//


#include <QApplication>
#include <editor/application/EditorApplication.h>
#include "window/MainWindow.h"

using namespace sky::editor;

int main(int argc, char *argv[])
{
    EditorApplication a(argc, argv);
    a.Setup();

    sky::editor::MainWindow mainWindow;
    mainWindow.show();

    auto ret = a.exec();
    a.Shutdown();

    return ret;
}


//
// Created by Zach Lee on 2021/12/12.
//

#include "window/MainWindow.h"
#include <QApplication>
#include <editor/application/EditorApplication.h>

using namespace sky::editor;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    EditorApplication editorApp;
    sky::StartInfo    startInfo = {};
    startInfo.appName           = "SkyEditor";
    editorApp.Init(startInfo);

    sky::editor::MainWindow mainWindow;
    mainWindow.show();
    return a.exec();
}

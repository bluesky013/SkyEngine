
//
// Created by Zach Lee on 2021/12/12.
//

#include "window/MainWindow.h"
#include <QApplication>
#include <editor/application/EditorApplication.h>
#include <framework/platform/PlatformBase.h>

using namespace sky::editor;

int main(int argc, char *argv[])
{
    sky::Platform* platform = sky::Platform::Get();
    if (!platform->Init({})) {
        return -1;
    }

    QApplication a(argc, argv);

    EditorApplication editorApp;
    if (!editorApp.Init(argc, argv)) {
        return 0;
    }

    sky::editor::MainWindow mainWindow;
    mainWindow.show();
    return a.exec();
}

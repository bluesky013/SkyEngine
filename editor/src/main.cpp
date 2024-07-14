
//
// Created by Zach Lee on 2021/12/12.
//

#include "window/MainWindow.h"
#include <QApplication>
#include <QSplashScreen>
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

    auto splashPath = editorApp.GetEngineFs()->GetPath();
    splashPath /= "assets/splash/test.png";

    editorApp;
    QPixmap pixmap(splashPath.GetStr().c_str());
    QSplashScreen splash(pixmap);
    splash.show();
    a.processEvents();

    sky::editor::MainWindow mainWindow;
    mainWindow.show();
    splash.finish(&mainWindow);

    return a.exec();
}


//
// Created by Zach Lee on 2021/12/12.
//

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

    EditorApplication editorApp(argc, argv);
    if (!editorApp.Init(argc, argv)) {
        return 0;
    }

    return editorApp.exec();
}

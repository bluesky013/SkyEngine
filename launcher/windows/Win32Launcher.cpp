//
// Created by Zach Lee on 2021/11/9.
//

#include "application/Application.h"
#include <windows.h>
#include <iostream>

void ProcessCommand(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        auto str = argv[i];
        std::cout << str << std::endl;
    }
}

int main(int argc, char** argv)
{
    ProcessCommand(argc, argv);

    sky::StartInfo start = {};
    start.appName = "Win32Launcher";
    start.modules = {
        "SampleModule"
    };

    sky::Application app;
    if (app.Init(start)) {
        app.Mainloop();
    }

    app.Shutdown();

    return 0;
}
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

    sky::Application app;
    app.Init();
    app.Mainloop();

    return 0;
}
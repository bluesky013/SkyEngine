//
// Created by Zach Lee on 2021/12/12.
//

#pragma once
#include <QApplication>
#include <engine/SkyEngine.h>
#include <framework/environment/Environment.h>

namespace sky::editor {

    class EditorApplication : public QApplication {
        Q_OBJECT
    public:
        EditorApplication(int &argc, char **argv, int flag = ApplicationFlags)
            : QApplication(argc, argv, flag)
            , engineInstance(nullptr)
        {
        }

        ~EditorApplication() = default;

        void Setup();

        void Shutdown();

    private:
        SkyEngine* engineInstance;
        Environment* env;
    };

}

//
// Created by Zach Lee on 2021/12/12.
//

#pragma once
#include <QApplication>

namespace sky {
    class SkyEngine;
}

namespace sky::editor {

    class EditorApplication : public QApplication {
        Q_OBJECT
    public:
        EditorApplication(int &argc, char **argv, int flag = ApplicationFlags)
            : QApplication(argc, argv, flag)
            , engine(nullptr)
        {
        }

        ~EditorApplication() = default;

        void Setup();

        void Shutdown();

    private:
        SkyEngine* engine;
    };

}

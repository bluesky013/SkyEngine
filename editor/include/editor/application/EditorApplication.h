//
// Created by Zach Lee on 2021/12/12.
//

#pragma once
#include <framework/application/Application.h>
#include <core/file/FileSystem.h>
#include <QTimer>
#include <QObject>

namespace sky::editor {

    class EditorApplication : public QObject, public Application {
        Q_OBJECT
    public:
        EditorApplication();
        ~EditorApplication() override;

        bool Init(int argc, char **argv) override;
    private:
        void LoadConfigs() override;

        QTimer *timer = nullptr;

        NativeFileSystemPtr workFs;
        NativeFileSystemPtr engineFs;
    };

}

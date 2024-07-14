//
// Created by Zach Lee on 2021/12/12.
//

#pragma once
#include <framework/application/Application.h>
#include <core/file/FileSystem.h>
#include <QTimer>
#include <QObject>
#include <QSplashScreen>

namespace sky::editor {

    class EditorApplication : public QObject, public Application {
        Q_OBJECT
    public:
        EditorApplication();
        ~EditorApplication() override;

        bool Init(int argc, char **argv) override;

        const NativeFileSystemPtr &GetWorkFs() const { return workFs; }
        const NativeFileSystemPtr &GetEngineFs() const { return engineFs; }
    private:
        void LoadConfigs() override;
        void LoadFromJson(std::unordered_map<std::string, ModuleInfo> &);

        QTimer *timer = nullptr;

        NativeFileSystemPtr workFs;
        NativeFileSystemPtr engineFs;
    };

}

//
// Created by Zach Lee on 2022/9/25.
//

#pragma once

#include <core/file/FileSystem.h>
#include <framework/application/Application.h>
#include <framework/project/ProjectDescriptor.h>
#include <framework/window/NativeWindow.h>

#include <string>

namespace sky {

    // Writable/project config takes precedence over read-only packaged config.
    FilePtr OpenGameConfig(const FileSystemPtr &workFs, const FileSystemPtr &bundleFs);

    class GameApplication : public Application {
    public:
        GameApplication()           = default;
        ~GameApplication() override = default;

        bool Init(int argc, char **argv) override;
        bool PreInit() override;
        void PostInit() override;

        void PreTick() override;

        NativeWindow *GetWindow() const
        {
            return nativeWindow.get();
        }

        void *GetMainWindowHandle() const override;

    protected:
        // Parses `--project <path>`; when present the project directory becomes the
        // work file system (its configs/ and assets/) and its modules are loaded.
        void ParseStartArgs() override;

    private:
        bool LoadConfigs() override;

        std::unique_ptr<NativeWindow> nativeWindow;
        uint32_t                      width  = 1024;
        uint32_t                      height = 1024;

        FileSystemPtr     workFs;
        ProjectDescriptor project;
        bool              hasProject = false;
    };

} // namespace sky

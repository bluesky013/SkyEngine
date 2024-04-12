import os.path


class ProjectBuilder:
    def __init__(self, project_path, engine_path):
        self.projectPath = project_path
        self.enginePath = engine_path

    def config_project(self):
        self.init_project_directory()

    def config_native(self):
        pass

    def config_asset(self):
        pass

    def config_config(self):
        pass

    def init_project_directory(self):
        asset_path = os.path.join(self.projectPath, "assets")
        config_path = os.path.join(self.projectPath, "config")
        native_path = os.path.join(self.projectPath, "native")

        path_list = [(asset_path, self.config_native),
                     (config_path, self.config_native),
                     (native_path, self.config_native)]
        for path, fn in path_list:
            if not os.path.exists(path):
                os.mkdir(path, 0o777)
                fn()

        engine_asset_path = os.path.join(self.projectPath, "engine_assets")
        engine_source_asset_path = os.path.join(self.enginePath, "assets")
        os.symlink(engine_source_asset_path, engine_asset_path, True)

        engine_runtime_path = os.path.join(self.projectPath, "runtime")
        project_runtime_link_path = os.path.join(self.projectPath, "native", "runtime")
        os.symlink(engine_runtime_path, project_runtime_link_path, True)
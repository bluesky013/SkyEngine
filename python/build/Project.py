import os.path
from build.Presets import CmakePreset, COMMON_PRESET_TEMP


def create_symlink(src, dst):
    if os.path.exists(dst):
        os.rmdir(dst)
    os.symlink(src, dst, True)


class PlatformConfig:
    def __init__(self):
        self.enable = False

    def set_enable(self, enable):
        self.enable = enable


class BuildConfig:
    def __init__(self):
        pass


class ProjectConfig:
    def __init__(self, project_path):
        self.projectPath = project_path
        self.builds = list()
        self.cmakePreset = CmakePreset()
        self.config_file_name = 'project_config.json'
        self.preset_file_name = 'CMakePresets.json'

    def save_config(self):
        self.cmakePreset.save(os.path.join(self.projectPath, self.preset_file_name))

    def load_config(self):
        self.cmakePreset.load(os.path.join(self.projectPath, self.preset_file_name))

    def update_path(self, engine_path):
        commonPreset = None
        for configPreset in self.cmakePreset.data['configurePresets']:
            if configPreset['name'] == 'common':
                commonPreset = configPreset
                configPreset['cacheVariables']['ENGINE_ROOT'] = engine_path
                break

        if commonPreset is None:
            COMMON_PRESET_TEMP['cacheVariables']['ENGINE_ROOT'] = engine_path
            self.cmakePreset.data['configurePresets'].append(COMMON_PRESET_TEMP)


class ProjectBuilder:
    def __init__(self, project_path, engine_path):
        self.projectPath = project_path
        self.enginePath = engine_path
        self.config = ProjectConfig(self.projectPath)
        self.config.load_config()
        self.config.update_path(engine_path)

    def init_project(self):
        self.init_project_directory()

    def update_project(self):
        self.update_project_symlink()
        pass

    def config_native(self):
        pass

    def config_asset(self):
        pass

    def config_config(self):
        pass

    def update_project_symlink(self):
        engine_source_asset_path = os.path.join(self.enginePath, "assets")
        engine_asset_link_path = os.path.join(self.projectPath, "engine_assets")
        create_symlink(engine_source_asset_path, engine_asset_link_path)

        engine_runtime_path = os.path.join(self.enginePath, "runtime")
        project_runtime_link_path = os.path.join(self.projectPath, "native", "runtime")
        create_symlink(engine_runtime_path, project_runtime_link_path)

        engine_launcher_path = os.path.join(self.enginePath, "launcher")
        project_launcher_link_path = os.path.join(self.projectPath, "native", "launcher")
        create_symlink(engine_launcher_path, project_launcher_link_path)

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

        self.update_project_symlink()

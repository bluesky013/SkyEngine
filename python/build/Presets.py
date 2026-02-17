import json
import os.path
from typing import List

PLATFORM_LIST = [
    'Windows',
    'Android',
    'Darwin',
    'IOS',
    'Linux',
]

PLATFORM_ARCH_LIST = {
    'Windows': ['x64'],
    'Android': ['arm64-v8a', 'armeabi-v7a', 'x86'],
    'Darwin': ['x86', 'arm64'],
    'IOS': ['arm64'],
    'Linux': ['x86_64'],
}

BUILD_TYPE_LIST = ['Debug', 'Release']

PLATFORM_AVAILABLE_LIST = {
    'Windows': ['Windows', 'Android'],
    'Darwin': ['Darwin', 'IOS', 'Android'],
    'Linux': ['Linux'],
}

GENERATOR_LIST = {
    'Windows': ['Visual Studio 17 2022', 'Visual Studio 16 2019'],
    'Darwin': ['Xcode'],
    'IOS': ['Xcode'],
    'Android': ['Ninja'],
    'Linux': ['Ninja', 'Unix Makefiles'],
}

COMMON_PRESET_TEMP = {
    'name': 'common',
    'hidden': True,
    'cacheVariables': {
        'ENGINE_ROOT': ''
    }
}


class BuildPreset:
    def __init__(self):
        self.data = {}


class ConfigPreset:
    def __init__(self):
        self.data = {}

    def get_display_name(self):
        return self.data['name']

    def set_name(self, name):
        self.data['name'] = name

    def set_generator(self, generator):
        self.data['generator'] = generator

    def set_description(self, description):
        self.data['description'] = description

    def add_cache_variable(self, key, val):
        if 'cacheVariables' not in self.data:
            self.data['cacheVariables'] = {}
        self.data['cacheVariables'][key] = val


class WindowsConfigPreset(ConfigPreset):

    def __init__(self):
        ConfigPreset.__init__(self)

    def get_display_name(self):
        return '%s-%s-%s' % (self.data['name'],
                             self.data['cacheVariables']['CMAKE_BUILD_TYPE'],
                             self.data['architecture']['value'])

    def set_arch(self, arch):
        self.data['architecture'] = {'value': '%s' % arch, 'strategy': 'set'}


class AndroidConfigPreset(ConfigPreset):
    def __init__(self):
        ConfigPreset.__init__(self)

    def get_display_name(self):
        return '%s-%s-%s' % (self.data['name'],
                             self.data['cacheVariables']['CMAKE_BUILD_TYPE'],
                             self.data['cacheVariables']['CMAKE_ANDROID_ARCH_ABI'])


class DarwinConfigPreset(ConfigPreset):
    def __init__(self):
        ConfigPreset.__init__(self)


class IOSConfigPreset(ConfigPreset):
    def __init__(self):
        ConfigPreset.__init__(self)


class CmakePreset:
    def __init__(self):
        self.data = {'version': 3,
                     'cmakeMinimumRequired': {'major': 3, 'minor': 19, 'patch': 0},
                     'configurePresets': [],
                     'buildPresets': []}

    def add_preset(self, preset: ConfigPreset):
        for current in self.data['configurePresets']:
            if current['name'] == preset.data['name']:
                return False
        self.data['configurePresets'].append(preset.data)

        preset.data['hidden'] = False
        preset.data['inherits'] = 'common'
        preset.data['displayName'] = preset.get_display_name()
        preset.data['binaryDir'] = '${sourceDir}/build/%s' % preset.data['displayName']
        buildPreset = BuildPreset()
        buildPreset.data['name'] = preset.data['name']
        buildPreset.data['configurePreset'] = preset.data['name']
        buildPreset.data['description'] = preset.data['description']
        buildPreset.data['configuration'] = preset.data['cacheVariables']['CMAKE_BUILD_TYPE']
        buildPreset.data['displayName'] = preset.data['displayName']
        self.data['buildPresets'].append(buildPreset.data)
        return True

    def load(self, path):
        if not os.path.exists(path):
            return
        with open(path, 'r') as f:
            self.data = json.load(f)

    def save(self, path):
        with open(path, 'w', encoding='utf-8') as f:
            json.dump(self.data, f, indent=4)

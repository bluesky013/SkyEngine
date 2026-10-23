"""Build-policy and tool-selection regressions without a platform SDK."""

import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('package_project', ROOT / 'python/package_project.py')
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)


class DevelopToolDiscoveryTest(unittest.TestCase):
    def test_prefers_develop_and_preserves_legacy_fallbacks(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for name, finder in (('AssetTool', PACKAGE.find_asset_tool), ('ShaderCookTool', PACKAGE.shader_cook_tool)):
                suffix = '.exe' if PACKAGE.os.name == 'nt' else ''
                for configuration in ('', 'RelWithDebInfo', 'Release', 'Debug', 'Develop'):
                    tool = root / 'output/bin' / configuration / (name + suffix)
                    tool.parent.mkdir(parents=True, exist_ok=True)
                    tool.touch()
                    self.assertEqual(finder(root), tool)
                (root / 'output/bin/Develop' / (name + suffix)).unlink()
                self.assertEqual(finder(root), root / 'output/bin/Debug' / (name + suffix))


@unittest.skipUnless(shutil.which('ninja'), 'Ninja is required for SDK-free configuration tests')
class DevelopConfigurationTest(unittest.TestCase):
    def configure(self, root, generator, *args):
        source = root / 'source'
        source.mkdir(exist_ok=True)
        (source / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.21)
project(Policy NONE)
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")
include("@ROOT@/cmake/build_configurations.cmake")
include("@ROOT@/cmake/thirdparty_helpers.cmake")
set(example_PATH "${CMAKE_CURRENT_SOURCE_DIR}/example")
set(CMAKE_STATIC_LIBRARY_PREFIX "lib")
set(CMAKE_STATIC_LIBRARY_SUFFIX ".a")
sky_3rd_static(example LIBS first second DEBUG_SUFFIX d)
add_library(Engine STATIC IMPORTED)
set_target_properties(Engine PROPERTIES IMPORTED_CONFIGURATIONS "Develop;Release"
    IMPORTED_LOCATION_DEVELOP "develop.a" IMPORTED_LOCATION_RELEASE "release.a")
add_library(Prebuilt STATIC IMPORTED)
set_target_properties(Prebuilt PROPERTIES IMPORTED_CONFIGURATIONS Release IMPORTED_LOCATION_RELEASE "prebuilt.a")
get_directory_property(definitions COMPILE_DEFINITIONS)
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/$<CONFIG>.txt" CONTENT
"${definitions}\\n$<GENEX_EVAL:$<TARGET_PROPERTY:3rdParty::example,INTERFACE_LINK_LIBRARIES>>\\n$<TARGET_FILE:Engine>\\n$<TARGET_FILE:Prebuilt>\\n")
file(WRITE "${CMAKE_BINARY_DIR}/flags.txt" "${CMAKE_CXX_FLAGS_DEVELOP}")
'''.replace('@ROOT@', ROOT.as_posix()), encoding='utf-8')
        result = subprocess.run(['cmake', '-S', str(source), '-B', str(root / 'build'), '-G', generator, *args],
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        return result

    def test_multi_configuration_defaults_and_dependencies(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            result = self.configure(root, 'Ninja Multi-Config', '-DSKY_DEVELOP=ON')
            self.assertEqual(result.returncode, 0, result.stdout)
            for configuration, assertions, validation in (('Debug', 1, 1), ('Develop', 1, 0),
                                                         ('Release', 0, 0), ('RelWithDebInfo', 0, 0)):
                output = (root / 'build' / (configuration + '.txt')).read_text()
                self.assertIn('SKY_ENABLE_ASSERT=' + str(assertions), output)
                self.assertIn('SKY_ENABLE_GPU_VALIDATION=' + str(validation), output)
                self.assertIn('SKY_ENABLE_RESOURCE_NAME=1', output)
                for library in ('first', 'second'):
                    expected = '/Debug/lib' + library + 'd.a' if configuration == 'Debug' else '/Release/lib' + library + '.a'
                    self.assertIn(expected, output)
                if configuration == 'Develop':
                    self.assertIn('develop.a', output)
                    self.assertIn('prebuilt.a', output)

    def test_single_configuration_and_explicit_overrides(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            result = self.configure(root, 'Ninja', '-DCMAKE_BUILD_TYPE=Develop', '-DSKY_ASSERTIONS=OFF',
                                    '-DSKY_GPU_VALIDATION=ON', '-DSKY_RESOURCE_NAMES=OFF')
            self.assertEqual(result.returncode, 0, result.stdout)
            output = (root / 'build/Develop.txt').read_text()
            for definition in ('SKY_BUILD_DEVELOP=1', 'SKY_ENABLE_ASSERT=0',
                               'SKY_ENABLE_GPU_VALIDATION=1', 'SKY_ENABLE_RESOURCE_NAME=0'):
                self.assertIn(definition, output)
            self.assertEqual((root / 'build/flags.txt').read_text(), '-O2 -g -DNDEBUG')

    def test_invalid_diagnostic_is_rejected(self):
        for option in ('SKY_ASSERTIONS', 'SKY_GPU_VALIDATION', 'SKY_RESOURCE_NAMES'):
            with self.subTest(option=option), tempfile.TemporaryDirectory() as temp:
                result = self.configure(Path(temp), 'Ninja', '-D' + option + '=invalid')
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(option + ' must be AUTO, ON or OFF', result.stdout)

    def test_game_presets_keep_publishing_defaults(self):
        presets = json.loads((ROOT / 'engine/templates/game/native/CMakePresets.json').read_text())
        configure = {p['name']: p for p in presets['configurePresets']}
        build = {p['name']: p for p in presets['buildPresets']}
        for platform in ('windows', 'linux', 'macos', 'ios', 'android'):
            develop = platform + '-develop'
            self.assertEqual(configure[develop]['cacheVariables']['SKY_DEVELOP'], 'ON')
            self.assertEqual(build[develop].get('configuration', 'Develop'), 'Develop')
            self.assertEqual(build[platform].get('configuration', 'Release'), 'Release')


if __name__ == '__main__':
    unittest.main()
